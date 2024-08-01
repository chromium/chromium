// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sandbox.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/utility/safe_browsing/mac/hfs.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "chrome/utility/safe_browsing/mac/udif.h"
#include "sandbox/mac/seatbelt.h"

namespace {

// SafeDMG (crdmg) is a utility that can perform a list or extract operation
// for DMG files. It does so without using the OS X built-in support for DMG
// files, and it operates under a restrictive sandbox. This is suitable for
// examining DMG files of untrusted origins.
//
// Usage: crdmg file.dmg [unpack-path]
// If unpack-path is provided, it will extract the contents of file.dmg to the
// directory, creating it if it does not exit. Otherwise, it will merely list
// the contents of the DMG.
class SafeDMG {
 public:
  SafeDMG();

  SafeDMG(const SafeDMG&) = delete;
  SafeDMG& operator=(const SafeDMG&) = delete;

  ~SafeDMG();

  int Main(base::span<const char*> args);

 private:
  // Prepares for an unpack operation, setting up |unpack_dir_| for the given
  // |unpack_path|.
  bool PrepareUnpack(const char* unpack_path);

  // Engages the sandbox for the DMG operation (list or unpack).
  bool EnableSandbox();

  // Performs the actual DMG operation.
  bool ParseDMG();

  base::File dmg_file_;

  // If this is running an unpack, rather than just a list operation, this is
  // a directory FD under which all the contents are written.
  base::ScopedFD unpack_dir_;
};

SafeDMG::SafeDMG() : dmg_file_(), unpack_dir_() {}

SafeDMG::~SafeDMG() {}

int SafeDMG::Main(base::span<const char*> args) {
  if (args.size() != 2 && args.size() != 3) {
    fprintf(stderr, "Usage: %s file.dmg [unpack-directory]\n", args[0]);
    fprintf(stderr,
            "If no unpack-directory is specified, the tool will\n"
            "list the contents of the DMG.\n");
    return EXIT_FAILURE;
  }

  dmg_file_.Initialize(base::FilePath(args[1]),
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!dmg_file_.IsValid()) {
    LOG(ERROR) << "Failed to open " << args[1] << ": "
               << dmg_file_.error_details();
    return EXIT_FAILURE;
  }

  if (args.size() == 3 && !PrepareUnpack(args[2])) {
    return EXIT_FAILURE;
  }

  if (!EnableSandbox())
    return EXIT_FAILURE;

  if (!ParseDMG())
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}

bool SafeDMG::PrepareUnpack(const char* unpack_path) {
  // Check that |unpack_path| exists as a directory, or create it if it
  // does not.
  struct stat statbuf;
  if (stat(unpack_path, &statbuf) != 0) {
    if (errno == ENOENT) {
      if (mkdir(unpack_path, 0755) != 0) {
        PLOG(ERROR) << "mkdir " << unpack_path;
        return false;
      }
    } else {
      PLOG(ERROR) << "stat " << unpack_path;
      return false;
    }
  } else if (!S_ISDIR(statbuf.st_mode)) {
    LOG(ERROR) << unpack_path << " is not a directory";
    return false;
  }

  unpack_dir_.reset(HANDLE_EINTR(open(unpack_path, O_DIRECTORY)));
  if (!unpack_dir_.is_valid()) {
    PLOG(ERROR) << "open " << unpack_path;
    return false;
  }

  return true;
}

bool SafeDMG::EnableSandbox() {
  // To list DMG files, no sandbox rules are required.
  std::string sbox_profile("(version 1) (deny default)");

  // If unpacking, add a subpath exception for the destination directory.
  if (unpack_dir_.is_valid()) {
    // Get the path from the already-open FD, in case the argument was an
    // unresolved symlink.
    char unpack_path[PATH_MAX];
    if (fcntl(unpack_dir_.get(), F_GETPATH, unpack_path) != 0) {
      PLOG(ERROR) << "fcntl unpack_dir_";
      return false;
    }

    if (strchr(unpack_path, '"') != 0 || strchr(unpack_path, '\\') != 0) {
      LOG(ERROR) << "Unpack directory path can't contain quotes or backslashes";
      return false;
    }

    sbox_profile += base::StringPrintf(
        " (allow file-write* (subpath \"%s\"))", unpack_path);
  }

  std::string sbox_error;
  if (!sandbox::Seatbelt::Init(sbox_profile.c_str(), 0, &sbox_error)) {
    LOG(ERROR) << "Failed to initialize sandbox: " << sbox_error;
    return false;
  }

  return true;
}

bool SafeDMG::ParseDMG() {
  // This does not use safe_browsing::dmg::DMGIterator since that skips over
  // directory nodes. These nodes are needed for mkdir() when unpacking.
  safe_browsing::dmg::FileReadStream read_stream(dmg_file_.GetPlatformFile());
  safe_browsing::dmg::UDIFParser udif_parser(&read_stream);

  if (!udif_parser.Parse())
    return false;

  for (size_t i = 0; i < udif_parser.GetNumberOfPartitions(); ++i) {
    printf("=== Partition #%zu: %s ===\n", i,
           udif_parser.GetPartitionName(i).c_str());

    std::unique_ptr<safe_browsing::dmg::ReadStream> partition_stream(
        udif_parser.GetPartitionReadStream(i));
    safe_browsing::dmg::HFSIterator iterator(partition_stream.get());
    if (!iterator.Open()) {
      VLOG(1) << "Skipped since this is not an HFS partition";
      continue;
    }

    while (iterator.Next()) {
      std::string path = base::UTF16ToUTF8(iterator.GetPath());
      printf("%s\n", path.c_str());

      // If this is just a list operation, no more work is required. Otherwise,
      // unpack the files as well as listing them.
      if (!unpack_dir_.is_valid())
        continue;

      if (iterator.IsDecmpfsCompressed()) {
        LOG(WARNING) << "Skipping decmpfs-compressed file: "
                     << iterator.GetPath();
        continue;
      }
      if (iterator.IsHardLink()) {
        LOG(WARNING) << "Skipping hard link: " << path;
        continue;
      }
      if (iterator.IsSymbolicLink()) {
        LOG(WARNING) << "Skipping symbolic link: " << path;
        continue;
      }

      if (iterator.IsDirectory()) {
        if (mkdirat(unpack_dir_.get(), path.c_str(), 0755) && errno != EEXIST) {
          PLOG(ERROR) << "mkdirat " << path;
        }
      } else {
        base::ScopedFD unpacked(
            HANDLE_EINTR(openat(unpack_dir_.get(), path.c_str(),
                                O_WRONLY | O_CREAT | O_TRUNC, 0644)));
        if (!unpacked.is_valid()) {
          PLOG(ERROR) << "openat " << path;
          continue;
        }

        std::unique_ptr<safe_browsing::dmg::ReadStream> stream(
            iterator.GetReadStream());
        size_t read_this_pass = 0;
        do {
          uint8_t buf[4096];
          if (!stream->Read(buf, &read_this_pass)) {
            LOG(ERROR) << "Failed to read stream: " << path;
            unlinkat(unpack_dir_.get(), path.c_str(), 0);
            break;
          }

          int rv = HANDLE_EINTR(write(unpacked.get(), buf, read_this_pass));
          if (rv < 0 || static_cast<size_t>(rv) < read_this_pass) {
            if (rv < 0)
              PLOG(ERROR) << "write " << path;
            else
              LOG(ERROR) << "Short write: " << path;
            unlinkat(unpack_dir_.get(), path.c_str(), 0);
            break;
          }
        } while (read_this_pass != 0);
      }
    }
  }

  return true;
}

}  // namespace

int main(int argc, const char* argv[]) {
  SafeDMG safe_dmg;
  // SAFETY: argc and argv come from the OS and must be trusted.
  return safe_dmg.Main(
      UNSAFE_BUFFERS(base::span(argv, base::saturated_cast<size_t>(argc))));
}
