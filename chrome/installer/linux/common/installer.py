# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import glob
import logging
import os
import pathlib
import re
import shutil
import stat
import subprocess
import sys


def run_command(cmd, cwd=None, env=None, stdout=None):
    logging.info(" ".join(cmd))

    if os.environ.get("VERBOSE"):
        subprocess.check_call(cmd, cwd=cwd, env=env, stdout=stdout)
    else:
        stderr_dest = subprocess.PIPE
        stdout_dest = stdout if stdout is not None else subprocess.PIPE

        proc = subprocess.run(
            cmd,
            cwd=cwd,
            env=env,
            stdout=stdout_dest,
            stderr=stderr_dest,
            text=True,
        )

        if proc.returncode != 0:
            logging.error(f"Command failed: {' '.join(cmd)}")
            if stdout_dest == subprocess.PIPE and proc.stdout:
                logging.error(f"stdout:\n{proc.stdout}")
            if proc.stderr:
                logging.error(f"stderr:\n{proc.stderr}")
            sys.exit(proc.returncode)


def verify_package_deps(expected_deps, actual_deps):
    expected = set(expected_deps)
    actual = set(actual_deps)

    if expected == actual:
        return

    missing = sorted(list(expected - actual))
    extra = sorted(list(actual - expected))

    print("Dependency mismatch!", file=sys.stderr)
    if missing:
        print(f"Missing: {missing}", file=sys.stderr)
    if extra:
        print(f"Extra: {extra}", file=sys.stderr)
    sys.exit(1)


def normalize_channel(channel):
    if channel == "stable":
        return (
            "stable",
            "https://chromereleases.googleblog.com/search/label/Stable%20updates",
        )
    elif channel in ["beta", "testing"]:
        return (
            "beta",
            "https://chromereleases.googleblog.com/search/label/Beta%20updates",
        )
    elif channel in ["dev", "unstable", "alpha"]:
        return (
            "unstable",
            "https://chromereleases.googleblog.com/search/label/Dev%20updates",
        )
    elif channel == "canary":
        return "canary", "N/A"
    else:
        print(
            f"ERROR: '{channel}' is not a valid channel type.", file=sys.stderr)
        sys.exit(1)


def gen_changelog(context, staging_dir, deb_changelog):
    if deb_changelog.exists():
        deb_changelog.unlink()

    timestamp = int(context["BUILD_TIMESTAMP"])
    dt = datetime.datetime.fromtimestamp(timestamp, datetime.timezone.utc)
    # RFC 5322 format: Mon, 24 Nov 2025 14:00:00 +0000
    date_rfc5322 = dt.strftime("%a, %d %b %Y %H:%M:%S %z")
    context["DATE_RFC5322"] = date_rfc5322

    # Assuming SCRIPTDIR is provided in context and points to the debian
    # directory where changelog.template resides.
    script_dir = pathlib.Path(context["SCRIPTDIR"])
    process_template(script_dir / "changelog.template", deb_changelog, context)

    run_command([
        "debchange",
        "-a",
        "--nomultimaint",
        "-m",
        "--changelog",
        str(deb_changelog),
        f"Release Notes: {context['RELEASENOTES']}",
    ])

    gzlog_dir = (
        staging_dir /
        f"usr/share/doc/{context['PACKAGE']}-{context['CHANNEL']}")
    gzlog_dir.mkdir(parents=True, exist_ok=True)
    gzlog = gzlog_dir / "changelog.gz"

    with gzlog.open("wb") as f_out:
        if os.environ.get("VERBOSE"):
            subprocess.check_call(
                ["gzip", "-9n", "-c", str(deb_changelog)], stdout=f_out)
        else:
            subprocess.check_call(
                ["gzip", "-9n", "-c", str(deb_changelog)],
                stdout=f_out,
                stderr=subprocess.DEVNULL,
            )
    gzlog.chmod(0o644)


def _process_template_includes(input_file, include_stack):
    input_file = pathlib.Path(input_file)
    if input_file in include_stack:
        print(
            "ERROR: Possible cyclic include detected: "
            f"{include_stack} -> {input_file}",
            file=sys.stderr,
        )
        sys.exit(1)

    include_stack.append(input_file)
    input_dir = input_file.parent

    output_lines = []
    with input_file.open("r") as f:
        for line in f:
            match = re.match(r"^\s*@@include@@(.*)", line)
            if match:
                inc_file_name = match.group(1).strip()
                inc_file_path = input_dir / inc_file_name
                if not inc_file_path.exists():
                    print(
                        f"ERROR: Couldn't read include file: {inc_file_path}",
                        file=sys.stderr,
                    )
                    sys.exit(1)
                output_lines.append(
                    _process_template_includes(inc_file_path, include_stack))
            else:
                output_lines.append(line)

    include_stack.pop()
    return "".join(output_lines)


def process_template(input_file, output_file, context):
    content = _process_template_includes(input_file, [])

    for var_name, value in context.items():
        # Check if the variable is present in the content
        placeholder = f"@@{var_name}@@"
        if placeholder in content:
            content = content.replace(placeholder, str(value))

    with open(output_file, "w") as f:
        f.write(content)

    if "@@" in content:
        print(
            f"Error: Some placeholders remain unfilled in {output_file}:",
            file=sys.stderr,
        )
        for line in content.splitlines():
            if "@@" in line:
                print(line, file=sys.stderr)
        sys.exit(1)


class Installer:

    def __init__(
        self,
        output_dir,
        staging_dir,
        channel,
        branding,
        arch,
        target_os,
        is_official_build,
    ):
        self.output_dir = pathlib.Path(output_dir)
        self.staging_dir = pathlib.Path(staging_dir)
        self.channel = channel
        self.branding = branding
        self.arch = arch
        self.target_os = target_os
        self.is_official_build = is_official_build
        self.context = {}

    def initialize(self):
        # Load branding info
        filename = ("google-chrome.info" if self.branding == "google_chrome"
                    else "chromium-browser.info")
        info_file = self.output_dir / "installer/common" / filename

        with info_file.open("r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    key, val = line.split("=", 1)
                    if val.startswith('"') and val.endswith('"'):
                        val = val[1:-1]
                    self.context[key] = val

        # Load theme branding
        branding_file = self.output_dir / "installer/theme/BRANDING"
        with branding_file.open("r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    key, val = line.split("=", 1)
                    if val.startswith('"') and val.endswith('"'):
                        val = val[1:-1]
                    self.context[key] = val

        self.get_version_info()

        # Verify channel
        self.channel, self.context["RELEASENOTES"] = normalize_channel(
            self.channel)

        self.context["CHANNEL"] = self.channel
        self.context["VERSIONFULL"] = (
            f"{self.context['VERSION']}-{self.context['PACKAGE_RELEASE']}")

        self.context["PACKAGE_ORIG"] = self.context["PACKAGE"]
        self.context["USR_BIN_SYMLINK_NAME"] = (
            f"{self.context['PACKAGE']}-{self.channel}")

        if self.channel != "stable":
            self.context["INSTALLDIR"] += f"-{self.channel}"
            self.context["PACKAGE"] += f"-{self.channel}"
            self.context["MENUNAME"] += f" ({self.channel})"
            self.context[
                "RDN_DESKTOP"] = f"{self.context['RDN']}.{self.channel}"
        else:
            self.context["RDN_DESKTOP"] = self.context["RDN"]

    @property
    def shlib_perms(self):
        return self.context.get("SHLIB_PERMS", 0o755)

    def set_context(self, context):
        self.context.update(context)

    def prep_staging_common(self):
        install_dir = self.staging_dir / self.context["INSTALLDIR"].lstrip("/")
        dirs = [
            install_dir,
            self.staging_dir / "usr/bin",
            self.staging_dir / "usr/share/applications",
            self.staging_dir / "usr/share/appdata",
            self.staging_dir / "usr/share/gnome-control-center/default-apps",
            self.staging_dir / "usr/share/man/man1",
        ]
        for d in dirs:
            d.mkdir(parents=True, exist_ok=True)
            d.chmod(0o755)

    def get_version_info(self):
        version_file = self.output_dir / "installer" / "version.txt"
        with version_file.open("r") as f:
            for line in f:
                key, val = line.strip().split("=", 1)
                self.context[key] = val

        self.context["VERSION"] = (
            f"{self.context['MAJOR']}.{self.context['MINOR']}."
            f"{self.context['BUILD']}.{self.context['PATCH']}")
        self.context["PACKAGE_RELEASE"] = "1"

    def stage_install_common(self):
        logging.info(f"Staging common install files in '{self.staging_dir}'...")
        install_dir = self.staging_dir / self.context["INSTALLDIR"].lstrip("/")

        self._stage_binaries(install_dir)
        self._stage_resources(install_dir)
        self._stage_theme_icons(install_dir)
        self._stage_desktop_integration(install_dir)

        # Verify ELF binaries
        self._verify_elf_binaries(install_dir)
        self._verify_file_permissions()

    def _stage_binaries(self, install_dir):
        # app
        progname = self.context["PROGNAME"]
        stripped_file = self.output_dir / f"{progname}.stripped"
        self._install(stripped_file, install_dir / progname, mode=0o755)

        # crashpad
        stripped_file = self.output_dir / "chrome_crashpad_handler.stripped"
        self._install(
            stripped_file,
            install_dir / "chrome_crashpad_handler",
            mode=0o755,
        )

        # chrome-management-service
        stripped_file = self.output_dir / "chrome_management_service.stripped"
        self._install(
            stripped_file,
            install_dir / "chrome-management-service",
            mode=0o755,
        )

        # sandbox
        stripped_file = self.output_dir / f"{progname}_sandbox.stripped"
        self._install(
            stripped_file, install_dir / "chrome-sandbox", mode=0o4755)

        # Widevine CDM
        widevine_src = self.output_dir / "WidevineCdm"
        if widevine_src.is_dir():
            # Need to copy recursively
            widevine_dest = install_dir / "WidevineCdm"
            if widevine_dest.exists():
                shutil.rmtree(widevine_dest)
            shutil.copytree(widevine_src, widevine_dest)

            # Fix permissions
            for root, dirs, files in os.walk(widevine_dest):
                os.chmod(root, 0o755)
                for d in dirs:
                    os.chmod(os.path.join(root, d), 0o755)
                for f in files:
                    path = os.path.join(root, f)
                    if f == "libwidevinecdm.so":
                        os.chmod(path, self.shlib_perms)
                    else:
                        os.chmod(path, 0o644)

        # ANGLE
        if (self.output_dir / "libEGL.so").exists():
            for f in ["libEGL.so", "libGLESv2.so"]:
                self._install(
                    self.output_dir / f"{f}.stripped",
                    install_dir / f,
                    mode=self.shlib_perms,
                )

        if (self.output_dir / "libvulkan.so.1").exists():
            self._install(
                self.output_dir / "libvulkan.so.1.stripped",
                install_dir / "libvulkan.so.1",
                mode=self.shlib_perms,
            )

        if (self.output_dir / "libvk_swiftshader.so").exists():
            self._install(
                self.output_dir / "libvk_swiftshader.so.stripped",
                install_dir / "libvk_swiftshader.so",
                mode=self.shlib_perms,
            )
            self._install_into_dir(
                self.output_dir / "vk_swiftshader_icd.json",
                install_dir,
                mode=0o644,
            )

        if (self.output_dir / "liboptimization_guide_internal.so").exists():
            self._install(
                self.output_dir / "liboptimization_guide_internal.so.stripped",
                install_dir / "liboptimization_guide_internal.so",
                mode=self.shlib_perms,
            )

        # QT shim
        for qt_ver in ["5", "6"]:
            libname = f"libqt{qt_ver}_shim.so"
            if (self.output_dir / libname).exists():
                self._install(
                    self.output_dir / f"{libname}.stripped",
                    install_dir / libname,
                    mode=self.shlib_perms,
                )

        # libc++
        if (self.output_dir / "lib/libc++.so").exists():
            lib_dir = install_dir / "lib"
            lib_dir.mkdir(parents=True, exist_ok=True)
            lib_dir.chmod(0o755)
            self._install(
                self.output_dir / "lib/libc++.so",
                lib_dir / "libc++.so",
                mode=self.shlib_perms,
                strip=True,
            )

    def _stage_resources(self, install_dir):
        # resources
        self._install_into_dir(
            self.output_dir / "resources.pak",
            install_dir,
            mode=0o644,
            strip=False,
        )

        if (self.output_dir / "chrome_100_percent.pak").exists():
            self._install_into_dir(
                self.output_dir / "chrome_100_percent.pak",
                install_dir,
                mode=0o644,
            )
            self._install_into_dir(
                self.output_dir / "chrome_200_percent.pak",
                install_dir,
                mode=0o644,
            )
        else:
            self._install_into_dir(
                self.output_dir / "theme_resources_100_percent.pak",
                install_dir,
                mode=0o644,
            )
            self._install_into_dir(
                self.output_dir / "ui_resources_100_percent.pak",
                install_dir,
                mode=0o644,
            )

        # ICU
        self._install_into_dir(
            self.output_dir / "icudtl.dat", install_dir, mode=0o644)

        # V8 snapshot
        if (self.output_dir / "v8_context_snapshot.bin").exists():
            self._install_into_dir(
                self.output_dir / "v8_context_snapshot.bin",
                install_dir,
                mode=0o644,
            )
        else:
            self._install_into_dir(
                self.output_dir / "snapshot_blob.bin",
                install_dir,
                mode=0o644,
            )

        # l10n paks
        locales_dir = install_dir / "locales"
        locales_dir.mkdir(parents=True, exist_ok=True)
        locales_dir.chmod(0o755)
        for pak in (self.output_dir / "locales").glob("*.pak"):
            self._install_into_dir(pak, locales_dir, mode=0o644)

        # Privacy Sandbox Attestation
        psa_manifest = (
            self.output_dir /
            "PrivacySandboxAttestationsPreloaded/manifest.json")
        if psa_manifest.exists():
            psa_dir = install_dir / "PrivacySandboxAttestationsPreloaded"
            psa_dir.mkdir(parents=True, exist_ok=True)
            psa_dir.chmod(0o755)
            self._install_into_dir(psa_manifest, psa_dir, mode=0o644)
            self._install_into_dir(
                self.output_dir / "PrivacySandboxAttestationsPreloaded" /
                "privacy-sandbox-attestations.dat",
                psa_dir,
                mode=0o644,
            )

        # MEI Preload
        mei_manifest = self.output_dir / "MEIPreload/manifest.json"
        if mei_manifest.exists():
            mei_dir = install_dir / "MEIPreload"
            mei_dir.mkdir(parents=True, exist_ok=True)
            mei_dir.chmod(0o755)
            self._install_into_dir(mei_manifest, mei_dir, mode=0o644)
            self._install_into_dir(
                self.output_dir / "MEIPreload/preloaded_data.pb",
                mei_dir,
                mode=0o644,
            )

        # default apps
        default_apps_src = self.output_dir / "default_apps"
        if default_apps_src.is_dir():
            default_apps_dest = install_dir / "default_apps"
            if default_apps_dest.exists():
                shutil.rmtree(default_apps_dest)
            shutil.copytree(default_apps_src, default_apps_dest)
            for root, dirs, files in os.walk(default_apps_dest):
                os.chmod(root, 0o755)
                for d in dirs:
                    os.chmod(os.path.join(root, d), 0o755)
                for f in files:
                    os.chmod(os.path.join(root, f), 0o644)

    def _stage_theme_icons(self, install_dir):
        # app icons
        icon_suffix = ""
        if self.branding == "google_chrome":
            if self.channel == "beta":
                icon_suffix = "_beta"
            elif self.channel == "unstable":
                icon_suffix = "_dev"
            elif self.channel == "canary":
                icon_suffix = "_canary"

        icon_sizes = [16, 24, 32, 48, 64, 128, 256]
        logo_resources_png = []
        for size in icon_sizes:
            logo = f"product_logo_{size}{icon_suffix}.png"
            logo_resources_png.append(logo)
            src = self.output_dir / "installer/theme" / logo
            self._install(src, install_dir / logo, mode=0o644)
        self.context["LOGO_RESOURCES_PNG"] = " " + " ".join(logo_resources_png)

    def _stage_desktop_integration(self, install_dir):
        # CHROME_VERSION_EXTRA
        if self.branding == "google_chrome":
            version_extra_file = install_dir / "CHROME_VERSION_EXTRA"
            with version_extra_file.open("w") as f:
                f.write(self.channel + "\n")
            version_extra_file.chmod(0o644)

        # wrapper script
        wrapper_file = install_dir / self.context["PACKAGE"]
        process_template(
            self.output_dir / "installer/common/wrapper",
            wrapper_file,
            self.context,
        )
        wrapper_file.chmod(0o755)
        logging.info(f"DEBUG: Chmod 755 {wrapper_file}. "
                     f"Stat: {oct(stat.S_IMODE(wrapper_file.stat().st_mode))}")

        # symlink for PACKAGE_ORIG
        package_orig = self.context.get("PACKAGE_ORIG")
        if package_orig:
            link_path = install_dir / package_orig
            if not link_path.exists():
                os.symlink(
                    os.path.join(self.context["INSTALLDIR"],
                                 self.context["PACKAGE"]),
                    link_path,
                )

        # /usr/bin symlink
        usr_bin_symlink_name = self.context.get("USR_BIN_SYMLINK_NAME")
        if usr_bin_symlink_name:
            link_path = self.staging_dir / "usr/bin" / usr_bin_symlink_name
            if link_path.is_symlink() or link_path.exists():
                link_path.unlink()
            os.symlink(
                os.path.join(self.context["INSTALLDIR"],
                             self.context["PACKAGE"]),
                link_path,
            )

        # URI_SCHEME
        uri_scheme = ""
        if self.branding == "google_chrome":
            if self.channel not in ["beta", "unstable", "canary"]:
                uri_scheme = "x-scheme-handler/google-chrome;"
        else:
            uri_scheme = "x-scheme-handler/chromium;"
        self.context["URI_SCHEME"] = uri_scheme

        # desktop integration
        self._install_into_dir(
            self.output_dir / "xdg-mime", install_dir, mode=0o755)
        self._install_into_dir(
            self.output_dir / "xdg-settings", install_dir, mode=0o755)

        appdata_file = (
            self.staging_dir / "usr/share/appdata" /
            f"{self.context['PACKAGE']}.appdata.xml")
        process_template(
            self.output_dir / "installer/common/appdata.xml.template",
            appdata_file,
            self.context,
        )
        appdata_file.chmod(0o644)

        desktop_file = (
            self.staging_dir / "usr/share/applications" /
            f"{self.context['PACKAGE']}.desktop")
        self.context["EXTRA_DESKTOP_ENTRIES"] = ""
        process_template(
            self.output_dir / "installer/common/desktop.template",
            desktop_file,
            self.context,
        )
        desktop_file.chmod(0o644)

        rdn_desktop_file = (
            self.staging_dir / "usr/share/applications" /
            f"{self.context['RDN_DESKTOP']}.desktop")
        self.context["EXTRA_DESKTOP_ENTRIES"] = (
            f"# This is the same as {self.context['PACKAGE']}.desktop except "
            "NoDisplay=true prevents\n"
            "# duplicate menu entries. This is required to match "
            "the application ID\n"
            "# used by XDG desktop portal, which has stricter "
            "naming requirements.\n"
            "# The old desktop file is kept to preserve default "
            "browser settings.\n"
            "NoDisplay=true\n")
        process_template(
            self.output_dir / "installer/common/desktop.template",
            rdn_desktop_file,
            self.context,
        )
        rdn_desktop_file.chmod(0o644)

        default_app_file = (
            self.staging_dir / "usr/share/gnome-control-center/default-apps" /
            f"{self.context['PACKAGE']}.xml")
        process_template(
            self.output_dir / "installer/common/default-app.template",
            default_app_file,
            self.context,
        )
        default_app_file.chmod(0o644)

        default_app_block_file = install_dir / "default-app-block"
        process_template(
            self.output_dir / "installer/common/default-app-block.template",
            default_app_block_file,
            self.context,
        )
        default_app_block_file.chmod(0o644)

        # documentation
        man_page = (
            self.staging_dir / "usr/share/man/man1" /
            f"{self.context['USR_BIN_SYMLINK_NAME']}.1")
        process_template(
            self.output_dir / "installer/common/manpage.1.in",
            man_page,
            self.context,
        )
        run_command(["gzip", "-9nf", str(man_page)])
        (man_page.parent / (man_page.name + ".gz")).chmod(0o644)

        # Link for stable channel app-without-channel case
        package_man_page = (
            self.staging_dir / "usr/share/man/man1" /
            f"{self.context['PACKAGE']}.1.gz")
        if not package_man_page.exists():
            os.symlink(
                f"{self.context['USR_BIN_SYMLINK_NAME']}.1.gz",
                package_man_page,
            )

    def _install_into_dir(self, src, dest_dir, mode=None, strip=False):
        dest = dest_dir / src.name
        self._install(src, dest, mode, strip)

    def _install(self, src, dest, mode=None, strip=False):
        dest.parent.mkdir(parents=True, exist_ok=True)

        shutil.copy(src, dest)
        if strip:
            run_command(["strip", str(dest)])
        if mode is not None:
            dest.chmod(mode)

    def _verify_elf_binaries(self, install_dir):
        unstripped = []
        rpath_bins = []
        elf_outside = []

        for root, _, files in os.walk(self.staging_dir):
            for f in files:
                path = pathlib.Path(root) / f
                if path.is_symlink():
                    continue

                # Check if ELF
                # file command is robust.
                try:
                    output = subprocess.check_output(
                        ["file", "-b", str(path)], text=True)
                except subprocess.CalledProcessError:
                    continue

                if "ELF" in output:
                    # Check if outside install dir
                    if not str(path).startswith(str(install_dir)):
                        elf_outside.append(path)

                    if "not stripped" in output:
                        unstripped.append(path)

                    if self.target_os != "chromeos":
                        # Check RPATH
                        try:
                            readelf_out = subprocess.check_output(
                                ["readelf", "-d", str(path)], text=True)
                            if "(RPATH)" in readelf_out:
                                rpath_bins.append(path.name)
                        except subprocess.CalledProcessError:
                            pass

        if unstripped:
            print(
                f"ERROR: Found unstripped ELF files: {unstripped}",
                file=sys.stderr,
            )
            sys.exit(1)

        if rpath_bins:
            print(
                f"ERROR: Found binaries with RPATH set: {rpath_bins}",
                file=sys.stderr,
            )
            sys.exit(1)

        if elf_outside:
            print(
                f"ERROR: Found {len(elf_outside)} ELF binaries outside of "
                f"{install_dir}",
                file=sys.stderr,
            )
            sys.exit(1)

    def _verify_file_permissions(self):
        # Verify file permissions
        on_cog = False
        if not self.is_official_build:
            # On Cog, permission is always 0664 or 0775
            if os.getcwd().startswith("/google/cog/cloud/"):
                on_cog = True
                print(
                    "INFO: build on Cog. relax permission for group writable",
                    file=sys.stderr,
                )

        for root, dirs, files in os.walk(self.staging_dir):
            # Check directories
            for d in dirs:
                pass

            # We iterate everything.
            for item in dirs + files:
                path = pathlib.Path(root) / item
                st = path.lstat()  # lstat to not follow symlinks
                actual_perms = stat.S_IMODE(st.st_mode)

                base_name = item
                expected_perms = 0o644  # Default

                if path.is_dir():
                    expected_perms = 0o755
                elif path.is_symlink():
                    target = os.readlink(path)
                    if target.startswith("/"):
                        expect_exists = self.staging_dir / target.lstrip("/")
                    else:
                        expect_exists = path.parent / target

                    if not os.path.lexists(expect_exists):
                        print(
                            f"Broken symlink: {path} -> {expect_exists}",
                            file=sys.stderr,
                        )
                        sys.exit(1)
                    # Skip permission check for symlinks
                    continue

                # Get file type
                try:
                    file_type = subprocess.check_output(
                        ["file", "-b", str(path)], text=True)
                except subprocess.CalledProcessError:
                    file_type = ""

                if base_name == "chrome-management-service":
                    expected_perms = 0o755
                elif base_name == "chrome-sandbox":
                    expected_perms = 0o4755
                elif "shell script" in file_type:
                    expected_perms = 0o755
                elif "ELF" in file_type:
                    if base_name.endswith(".so") or ".so." in base_name:
                        expected_perms = self.shlib_perms
                    else:
                        expected_perms = 0o755

                if expected_perms != actual_perms:
                    ok = False
                    relaxed_expected_perms = expected_perms
                    if on_cog:
                        if expected_perms == 0o4755:
                            relaxed_expected_perms = 0o775
                        elif expected_perms == 0o644:
                            relaxed_expected_perms = 0o664
                        elif expected_perms == 0o755:
                            relaxed_expected_perms = 0o775

                        if relaxed_expected_perms == actual_perms:
                            ok = True

                    if not ok:
                        msg = (
                            f"Expected permissions on {base_name} ({path}) to "
                            f"be {oct(expected_perms)}")
                        if on_cog:
                            msg += f" or {oct(relaxed_expected_perms)}"
                        msg += f", but they were {oct(actual_perms)}"
                        print(msg, file=sys.stderr)
                        sys.exit(1)
