// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// launcher_main.c implements a main method that launches
// `updater --server --service=update [--system] [logging flags]`.
// Because the launcher is sometimes used in a root setuid context, it has
// minimal dependencies and tries to harden the environment.
//
// If run with --test as the first argument, the launcher instead launches the
// updater with the `--test` flag instead of the `--server` flag. The updater
// will immediately exit in this case, but this is useful for testing the
// launcher.
//
// If run with --internal as the first argument, the launcher instead launches
// the updater with the `--service=update-internal` flag instead of the
// `--service=update` flag.
//
// In the system (setuid) context, the launcher verifies several security
// attributes of the binary it intends to launch, the path leading to the
// binary it intends to launch, and the non-chrootedness of its context; if any
// of these checks fail, it prints a diagnostic to stderr and returns a nonzero
// exit code (see <sysexits.h>).
//
// It isolates the subprocess from its calling environment by launching it into
// a new session, with a fixed environ and argv, with standard file handles
// pointing to /dev/null and all signal handling reset to default. macOS
// clears Mach exception ports before launching a setuid binary, so the
// launcher does not repeat this work. posix_spawn will fail (and the launcher
// will therefore print a diagnostic and return nonzero) if it cannot honor all
// of these settings.
//
// In the system context, the launcher resets its own bootstrap port to the
// privileged systemwide session, resets uid and gid to 0 (real, saved, and
// effective), resets rlimits to default values, resets its umask to 022,
// resets its working directory to '/', and resets its kernel security groups.
// It expects the subprocess to inherit all of these. It fails with a suitable
// diagnostic message and return value if any of these operations fail.
//
// This list of security checks and isolation mechanisms may not be exhaustive.
//
// If the launch is successful, it returns 0 (EX_OK). It does not become the
// subprocess and it does not wait for the subprocess to exit. It does not
// report the PID of the process it creates.

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <bootstrap.h>
#include <err.h>
#include <errno.h>
#include <libproc.h>
#include <limits.h>
#include <mach/exception_types.h>
#include <mach/task.h>
#include <mach/task_special_ports.h>
#include <mach/vm_param.h>
#include <machine/vmparam.h>
#include <membership.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/acl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslimits.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vnode.h>
#include <sysexits.h>
#include <unistd.h>

#include "chrome/updater/mac/launcher_constants.h"
#include "chrome/updater/updater_branding.h"

#define ARRAYSIZE(x) (sizeof(x) / sizeof(*(x)))

static bool StrAppend(char* dest, const char* suffix, size_t dest_size) {
  return strlcat(dest, suffix, dest_size) < dest_size;
}

// ErrSec exits the program with the specified return code, emitting an error
// message of the form <msg>: <error code>.
static __attribute__((noreturn)) void ErrSec(int rc,
                                             OSStatus error,
                                             const char* msg) {
  errx(rc, "%s: %d", msg, error);
}

// Checks whether extended POSIX ACLs allow write access on the specified path.
// ACLs for the specified principal are skipped -- this should be root's UUID.
static bool AclPermitsWrite(char const* const path, guid_t root_uuid) {
  acl_t acl = acl_get_link_np(path, ACL_TYPE_EXTENDED);
  if (!acl) {
    if (errno == ENOENT) {
      // No ACL is associated with this file.
      return false;
    }
    err(EX_OSERR, "couldn't get acl on %s", path);
  }

  acl_entry_t entry = NULL;
  for (int rc = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry); !rc;
       rc = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry)) {
    acl_tag_t tag = 0;
    if (acl_get_tag_type(entry, &tag)) {
      err(EX_OSERR, "getting ACL tag for %s", path);
    }
    if (tag != ACL_EXTENDED_ALLOW) {
      continue;
    }

    guid_t* principal_p = acl_get_qualifier(entry);
    if (!principal_p) {
      err(EX_OSERR, "getting ACL qualifier for %s", path);
    }
    if (!memcmp(principal_p->g_guid, root_uuid.g_guid, KAUTH_GUID_SIZE)) {
      // Skip ACLs that grant permission to root.
      acl_free(principal_p);
      continue;
    }
    acl_free(principal_p);

    uint64_t permset_mask = 0;
    if (acl_get_permset_mask_np(entry, &permset_mask)) {
      err(EX_OSERR, "getting ACL perms for %s", path);
    }
    if (permset_mask &
        (ACL_WRITE_DATA | ACL_ADD_FILE | ACL_DELETE | ACL_APPEND_DATA |
         ACL_ADD_SUBDIRECTORY | ACL_DELETE_CHILD | ACL_WRITE_ATTRIBUTES |
         ACL_WRITE_EXTATTRIBUTES | ACL_WRITE_SECURITY | ACL_CHANGE_OWNER)) {
      acl_free(acl);
      return true;
    }
  }
  acl_free(acl);
  return false;
}

// VerifyPathOrDie verifies that path (must be absolute) and all of its prefixes
// are owned by root and cannot be written others. It iterates in root-to-leaf
// order to mitigate the opportunity for time-of-check/time-of-use flaws, since
// at each step, we know the path so far can only have been changed out from
// under us by something with root permissions. It forbids symlinks.
//
// If verification fails, the program prints an error (to stderr) and exits.
static void VerifyPathOrDie(const char* const path, guid_t root_uuid) {
  if (path[0] != '/') {
    errx(EX_SOFTWARE, "path not absolute: %s", path);
  }

  size_t len = strlen(path);
  char* accumulating_path = calloc(strlen(path) + 1, 1);
  for (size_t i = 0; i < len; ++i) {
    accumulating_path[i] = path[i];
    if (i && path[i + 1] != '/' && path[i + 1] != '\0') {
      continue;
    }
    // We've reached root, some directory, or the full path; check it.
    struct stat attribs = {};
    if (lstat(accumulating_path, &attribs)) {
      err(EX_NOPERM, "can't stat %s", accumulating_path);
    }

    if (S_ISLNK(attribs.st_mode)) {
      errx(EX_OSFILE, "%s is a symlink", accumulating_path);
    }
    if (attribs.st_mode & 022) {
      errx(EX_OSFILE, "loose permissions (0%o) on %s", attribs.st_mode & 07777,
           accumulating_path);
    }
    if (attribs.st_uid) {
      errx(EX_CONFIG, "non-root user %u owns %s", attribs.st_uid,
           accumulating_path);
    }
    if (AclPermitsWrite(accumulating_path, root_uuid)) {
      errx(EX_CONFIG, "loose permissions (extended ACL) on %s",
           accumulating_path);
    }
  }
  free(accumulating_path);
}

static bool IsChrooted() {
  struct proc_vnodepathinfo vnodepathinfo = {};
  if (proc_pidinfo(getpid(), PROC_PIDVNODEPATHINFO, 0, &vnodepathinfo,
                   sizeof(vnodepathinfo)) < 0) {
    err(EX_OSERR, "proc_pidinfo");
  }

  return vnodepathinfo.pvi_rdir.vip_vi.vi_type == VDIR;
}

// An rlimit configuration for some specified resource. "which" is expected
// to be an RLIMIT constant.
typedef struct rlimit_config_struct {
  int which;
  struct rlimit rlimit;
} rlimit_config;

// Increase the limit for the resource defined by config->which to the values
// in config->soft (for rlim_cur) and config->hard (for rlim_max). This only
// ever increases limits; if the existing limit is already higher, it does
// not change the limit. If one part of the rlimit change is an increase and
// the other is not, it applies only the increase.
//
// If the getrlimit call to find existing values fails, this returns its
// return code (and does not change anything). Otherwise, it returns the
// return code from its call to setrlimit.
static int IncreaseRlimit(const rlimit_config* config) {
  struct rlimit lim;
  int rc = getrlimit(config->which, &lim);
  if (rc) {
    return rc;
  }
  if (lim.rlim_cur < config->rlimit.rlim_cur) {
    lim.rlim_cur = config->rlimit.rlim_cur;
  }
  if (lim.rlim_max < config->rlimit.rlim_max) {
    lim.rlim_max = config->rlimit.rlim_max;
  }
  // `cur` > `max` can occur for some `config`s, at least the maxproc one.
  if (lim.rlim_cur > lim.rlim_max) {
    lim.rlim_max = lim.rlim_cur;
  }
  return setrlimit(config->which, &lim);
}

// Array terminated with an rlimit_config with `which` set to
// kEndOfDefaultRlimits.  Omits RLIMIT_NPROC, since its correct value is
// machine-specific; we use sysctlbyname to get the real caps and call
// setrlimit separately for this.
static const rlimit_config default_rlimits[] = {
    {.which = RLIMIT_CORE,
     .rlimit = {.rlim_cur = DFLCSIZ, .rlim_max = MAXCSIZ}},
    {.which = RLIMIT_CPU,
     .rlimit = {.rlim_cur = RLIM_INFINITY, .rlim_max = RLIM_INFINITY}},
    {.which = RLIMIT_FSIZE,
     .rlimit = {.rlim_cur = RLIM_INFINITY, .rlim_max = RLIM_INFINITY}},
    {.which = RLIMIT_DATA,
     .rlimit = {.rlim_cur = DFLDSIZ, .rlim_max = MAXDSIZ}},
    {.which = RLIMIT_STACK,
     .rlimit = {.rlim_cur = DFLSSIZ, .rlim_max = MAXSSIZ - PAGE_MAX_SIZE}},
    {.which = RLIMIT_RSS,
     .rlimit = {.rlim_cur = RLIM_INFINITY, .rlim_max = RLIM_INFINITY}},
    {.which = RLIMIT_MEMLOCK,
     .rlimit = {.rlim_cur = RLIM_INFINITY, .rlim_max = RLIM_INFINITY}},
    {.which = RLIMIT_NOFILE,
     .rlimit = {.rlim_cur = NOFILE, .rlim_max = OPEN_MAX}},
};

// Check whether the code object referenced via distant_code is validly signed.
// Returns an OSStatus in the errSec... space describing the result of the
// check.
//
// Like many Security.h APIs, the SecStaticCodeRef argument (distant_code) can
// be a "live" SecCodeRef instead. If a live SecCodeRef is provided, this check
// runs against the running image rather than the file on disk.
static OSStatus CheckSignature(SecStaticCodeRef distant_code) {
  if (!distant_code) {
    errx(EX_SOFTWARE, "can't check signature that doesn't exist");
  }

  SecRequirementRef req;
  OSStatus rc = SecRequirementCreateWithString(
      // Magic numbers from
      // https://chromium.googlesource.com/chromium/src/+/7b1441f65d5bef3c0fe531809cccdeb40f9465c6/chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_mac.cc#49
      CFSTR("anchor apple generic and "
            "certificate 1[field.1.2.840.113635.100.6.2.6] and "
            "certificate leaf[field.1.2.840.113635.100.6.1.13] and "
            "certificate leaf[subject.OU] = "
            "\"" MAC_TEAM_IDENTIFIER_STRING "\" and "
            "identifier \"" MAC_BUNDLE_IDENTIFIER_STRING "\""),
      kSecCSDefaultFlags, &req);
  if (rc != errSecSuccess) {
    ErrSec(EX_UNAVAILABLE, rc, "can't create requirement");
  }

  // We now have the pieces we need: a security requirement and the code
  // signing object for the connecting process. Evaluate it, clean up our
  // various Core Foundation objects, then return the verdict. Use live
  // validity checks if possible.
  if (CFGetTypeID(distant_code) == SecCodeGetTypeID()) {
    SecCodeRef distant_live_code = (SecCodeRef)distant_code;
    rc = SecCodeCheckValidity(distant_live_code, kSecCSDefaultFlags, req);
  } else {
    // This really is a static code ref.
    rc = SecStaticCodeCheckValidity(distant_code,
                                    kSecCSCheckAllArchitectures |
                                        kSecCSCheckNestedCode |
                                        kSecCSStrictValidate,
                                    req);
  }

  CFRelease(req);
  return rc;
}

static void Harden(const char* target_path) {
  if (IsChrooted()) {
    err(EX_CONFIG, "chrooted");
  }
  if (setuid(0)) {
    err(EX_NOPERM, "can't setuid 0");
  }
  if (setgid(0)) {
    err(EX_NOPERM, "can't setgid 0");
  }

  // Check supported platforms. POSIX_SPAWN_SETSID doesn't work until
  // macOS 10.12, known internally to uname as 16.0.0.
  struct utsname sysinfo = {};
  if (uname(&sysinfo)) {
    err(EX_OSERR, "can't get uname");
  }
  char* scan_end = NULL;
  long os_major = strtol(sysinfo.release, &scan_end, 10);
  if (scan_end == sysinfo.release) {
    errx(EX_OSERR, "empty uname.release");
  }
  if (*scan_end != '.') {
    errx(EX_PROTOCOL, "malformed uname.release (%s)", sysinfo.release);
  }
  if (os_major < 16L) {
    errx(EX_UNAVAILABLE, "launcher requires macOS 10.12 or later");
  }

  // Set process count limits to system caps.
  int32_t maxproc = -1;
  size_t maxprocsz = sizeof(maxproc);
  if (sysctlbyname("kern.maxproc", &maxproc, &maxprocsz, NULL, 0)) {
    err(EX_UNAVAILABLE, "can't get kern.maxproc");
  }
  int32_t maxprocperuid = -1;
  size_t maxprocperuidsz = sizeof(maxprocperuid);
  if (sysctlbyname("kern.maxprocperuid", &maxprocperuid, &maxprocperuidsz, NULL,
                   0)) {
    err(EX_UNAVAILABLE, "can't get kern.maxprocperuid");
  }
  rlimit_config nproc = {
      .which = RLIMIT_NPROC,
      .rlimit = {.rlim_cur = maxprocperuid, .rlim_max = maxproc}};
  if (IncreaseRlimit(&nproc)) {
    err(EX_OSERR, "can't set rlimit %d", RLIMIT_NPROC);
  }

  // Reset other resource limits.
  for (size_t i = 0; i < ARRAYSIZE(default_rlimits); i++) {
    if (IncreaseRlimit(&default_rlimits[i])) {
      err(EX_OSERR, "can't set rlimit %d", default_rlimits[i].which);
    }
  }

  // Find the startup port, the bootstrap port for all daemons. We use this as
  // our bootstrap port so our requests to other root-level services can't be
  // intercepted by something running in a user context. The subprocess will
  // inherit this.
  //
  // The startup port is uniquely recognizable as the port that is its own
  // parent.
  //
  // Empirical observation indicates that each next_port is likely to leak; each
  // port acquires references not owned by this code (possibly from the MacOS
  // APIs themselves). This implementation does not depend on this property,
  // however.
  mach_port_t startup_port = MACH_PORT_NULL;
  mach_port_t next_port = bootstrap_port;
  // Give next_port its own reference count for bootstrap_port so it won't be
  // deallocated immediately if/when it "falls off" the end of this
  // traversal.
  kern_return_t kr = mach_port_mod_refs(mach_task_self(), bootstrap_port,
                                        MACH_PORT_RIGHT_SEND, 1);
  if (kr != KERN_SUCCESS && kr != KERN_INVALID_RIGHT) {
    errx(EX_OSERR, "can't add send count for bootstrap_port: %d", kr);
  }
  do {
    if (startup_port != MACH_PORT_NULL) {
      mach_port_deallocate(mach_task_self(), startup_port);
    }
    startup_port = next_port;
    kern_return_t bootstrap_err = bootstrap_parent(startup_port, &next_port);
    if (bootstrap_err != KERN_SUCCESS) {
      errx(EX_NOPERM, "bootstrap_parent: %d", bootstrap_err);
    }
  } while (startup_port != next_port);

  // Release the the extra retains: transfer one right, one retain from `start`,
  // and one retain from `next_port`.
  task_set_bootstrap_port(mach_task_self(), startup_port);
  mach_port_t old_bootstrap = bootstrap_port;
  bootstrap_port = startup_port;
  mach_port_deallocate(mach_task_self(), old_bootstrap);
  mach_port_deallocate(mach_task_self(), next_port);

  // initgroups and mbr_uid_to_uuid must be done only after bootstrap_port has
  // become the startup port, since they make calls to opendirectoryd; looking
  // it up can be redirected in a subset port, so using only the startup port
  // ensures we're talking to the real one.
  if (initgroups("root", 0)) {
    err(EX_OSERR, "can't initgroups");
  }
  guid_t root_uuid;
  if (mbr_uid_to_uuid(0, root_uuid.g_guid)) {
    err(EX_OSERR, "can't get root's uuid");
  }

  // Verify paths. verifyPath is recursive and prints out error messages on its
  // own if something goes wrong.
  VerifyPathOrDie(target_path, root_uuid);

  // Check signing.
  if (kCheckSigning) {
    CFStringRef cf_subprocess_path =
        CFStringCreateWithCString(NULL, target_path, kCFStringEncodingUTF8);
    CFURLRef subprocess_path_url = CFURLCreateWithFileSystemPath(
        NULL, cf_subprocess_path, kCFURLPOSIXPathStyle, false);
    CFRelease(cf_subprocess_path);
    SecStaticCodeRef subprocess_code;
    OSStatus os_rc = SecStaticCodeCreateWithPath(
        subprocess_path_url, kSecCSDefaultFlags, &subprocess_code);
    CFRelease(subprocess_path_url);
    if (os_rc != errSecSuccess) {
      ErrSec(EX_UNAVAILABLE, os_rc,
             "can't get code signing info for subprocess");
    }
    os_rc = CheckSignature(subprocess_code);
    if (os_rc != errSecSuccess) {
      ErrSec(EX_CONFIG, os_rc, "subprocess verification failed");
    }
    CFRelease(subprocess_code);

    CFStringRef cf_bundle_path =
        CFStringCreateWithCString(NULL, kBundlePath, kCFStringEncodingUTF8);
    CFURLRef bundle_path_url = CFURLCreateWithFileSystemPath(
        NULL, cf_bundle_path, kCFURLPOSIXPathStyle, false);
    SecStaticCodeRef bundle_code;
    os_rc = SecStaticCodeCreateWithPath(bundle_path_url, kSecCSDefaultFlags,
                                        &bundle_code);
    CFRelease(bundle_path_url);
    CFRelease(cf_bundle_path);
    if (os_rc != errSecSuccess) {
      ErrSec(EX_UNAVAILABLE, os_rc, "can't get code signing info for bundle");
    }
    os_rc = CheckSignature(bundle_code);
    if (os_rc != errSecSuccess) {
      ErrSec(EX_CONFIG, os_rc, "bundle verification failed");
    }
    CFRelease(bundle_code);
    // Signing checks have passed.
  }
}

static void Launch(
    bool is_system, bool is_qualifying, bool is_internal, const char* path) {
  if (chdir("/")) {
    err(EX_OSFILE, "can't chdir to /");
  }
  umask(022);
  alarm(0);

  // Configure posix_spawn.
  sigset_t empty_sigset;  // Zero-initialization of sigset_t is not portable.
  sigemptyset(&empty_sigset);
  sigset_t full_sigset;
  sigfillset(&full_sigset);
  posix_spawnattr_t spawn_attrs = NULL;
  int posix_err = posix_spawnattr_init(&spawn_attrs);
  if (posix_err) {
    errc(EX_UNAVAILABLE, posix_err, "can't init spawn attrs");
  }
  posix_err = posix_spawnattr_setflags(
      &spawn_attrs, POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK |
                        POSIX_SPAWN_SETSID | POSIX_SPAWN_CLOEXEC_DEFAULT);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't set spawn flags");
  }
  posix_err = posix_spawnattr_setsigdefault(&spawn_attrs, &full_sigset);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't set default signal handlers");
  }
  posix_err = posix_spawnattr_setsigmask(&spawn_attrs, &empty_sigset);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't clear signal mask");
  }

  cpu_type_t cpu_any = CPU_TYPE_ANY;
  size_t ocount = 0;
  posix_err = posix_spawnattr_setbinpref_np(&spawn_attrs, 1, &cpu_any, &ocount);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't set CPU_TYPE_ANY binpref");
  }
  posix_err = posix_spawnattr_setspecialport_np(&spawn_attrs, bootstrap_port,
                                                TASK_BOOTSTRAP_PORT);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't set bootstrap port");
  }
  // There is no need to clear task-level exception ports, because in the isRoot
  // context, macOS already did that as a consequence of launching a setuid
  // process. Refer to kern_exec.c in xnu source.

  posix_spawn_file_actions_t file_actions = NULL;
  posix_err = posix_spawn_file_actions_init(&file_actions);
  if (posix_err) {
    errc(EX_UNAVAILABLE, posix_err, "can't init file actions");
  }
  posix_err = posix_spawn_file_actions_addopen(&file_actions, STDIN_FILENO,
                                               "/dev/null", O_RDONLY, 0);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't point /dev/null to stdin");
  }
  posix_err = posix_spawn_file_actions_addopen(&file_actions, STDOUT_FILENO,
                                               "/dev/null", O_WRONLY, 0);
  if (posix_err) {
    errc(EX_OSERR, posix_err, "can't point stdout to /dev/null");
  }
  posix_err = posix_spawn_file_actions_adddup2(&file_actions, STDOUT_FILENO,
                                               STDERR_FILENO);
  if (posix_err) {
    errc(EX_OSERR, posix_err,
         "can't point stderr to /dev/null (as dup2 of stdout)");
  }

  char* const argv[] = {
      (char*)kExecutableName,  // posix_spawn will not overwrite the argv.
      is_qualifying ? "--test" : "--server",
      is_internal ? "--service=update-internal" : "--service=update",
      is_system ? "--system" : NULL,
      NULL};
  static char* const env[] = {"PWD=/", "PATH=/usr/bin:/bin:/usr/sbin:/sbin",
                              NULL};

  posix_err = posix_spawn(NULL, path, &file_actions, &spawn_attrs, argv, env);
  if (posix_err) {
    errc(EX_UNAVAILABLE, posix_err, "posix_spawn failed");
  }
}

void UserMain(uid_t euid, bool is_qualifying, bool is_internal) {
  // Find home directory.
  const char* home = getenv("HOME");
  if (!home) {
    // getpwuid is thread-safe on macOS. The program may become multi-threaded
    // once we invoke Apple APIs.
    struct passwd* pwd = getpwuid(euid);
    if (pwd) {
      home = pwd->pw_dir;
    }
  }
  if (!home) {
    err(EX_OSERR, "unable to find user homedir");
  }
  char path[PATH_MAX] = "";
  if (!StrAppend(path, home, ARRAYSIZE(path))) {
    err(EX_OSERR, "path to homedir is too long");
  }
  if (!StrAppend(path, kExecutablePath, ARRAYSIZE(path))) {
    err(EX_OSERR, "path to updater executable is too long");
  }

  Launch(false, is_qualifying, is_internal, path);
}

void SystemMain(bool is_qualifying, bool is_internal) {
  Harden(kExecutablePath);
  Launch(true, is_qualifying, is_internal, kExecutablePath);
}

int main(int argc, char** argv) {
  const uid_t euid = geteuid();
  bool is_qualifying = argc >= 2 && strcmp("--test", argv[1]) == 0;
  bool is_internal = argc >= 2 && strcmp("--internal", argv[1]) == 0;
  if (euid == 0) {
    SystemMain(is_qualifying, is_internal);
  } else {
    UserMain(euid, is_qualifying, is_internal);
  }
  return EX_OK;
}
