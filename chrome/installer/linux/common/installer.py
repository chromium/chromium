# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import dataclasses
import datetime
import glob
import logging
import os
import pathlib
import re
import shlex
import shutil
import stat
import string
import subprocess
import sys
import typing


class StandardPermissions:
    EXECUTABLE = 0o755
    REGULAR = 0o644
    SANDBOX = 0o4755


class ArtifactType:
    BINARY = "binary"
    DIRECTORY = "directory"
    GENERATED = "generated"
    ICON = "icon"
    RESOURCE = "resource"
    SYMLINK = "symlink"
    TEMPLATE = "template"


@dataclasses.dataclass
class Artifact:
    src: str | pathlib.Path
    dst: str | pathlib.Path
    artifact_type: str
    mode: int = StandardPermissions.REGULAR
    strip: bool = False
    is_optional: bool = False
    dst_base: str = "install_dir"  # install_dir or staging_dir
    symlink_target: str = ""
    content: str = ""
    template_context: dict | None = None
    compress: bool = False


class StagingContext:

    def __init__(self, *dirs: pathlib.Path) -> None:
        self.dirs = dirs

    def __enter__(self) -> None:
        for d in self.dirs:
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True, exist_ok=True)

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        for d in self.dirs:
            if d.exists():
                shutil.rmtree(d)


def parse_common_args(
    parser: argparse.ArgumentParser = None,) -> argparse.Namespace:
    if parser is None:
        parser = argparse.ArgumentParser()
    parser.add_argument(
        "-a", "--arch", required=True, help="package architecture")
    parser.add_argument(
        "-b", "--build-time", required=True, help="build timestamp")
    parser.add_argument(
        "-c", "--channel", required=True, help="package channel")
    parser.add_argument("-d", "--branding", required=True, help="branding")
    parser.add_argument(
        "-f", "--official", action="store_true", help="official build")
    parser.add_argument(
        "-o", "--output-dir", required=True, help="output directory")
    parser.add_argument("-t", "--target-os", required=True, help="target os")
    return parser


def run_command(
    cmd: list[str],
    cwd: pathlib.Path | None = None,
    env: dict[str, str] | None = None,
    stdout: typing.IO | int | None = None,
) -> None:
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


def verify_package_deps(expected_deps: list[str],
                        actual_deps: list[str]) -> None:
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


def normalize_channel(channel: str) -> tuple[str, str]:
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


def gen_changelog(config: "InstallerConfig",
                  deb_changelog: pathlib.Path) -> None:
    if deb_changelog.exists():
        deb_changelog.unlink()

    timestamp = int(config.build_timestamp)
    dt = datetime.datetime.fromtimestamp(timestamp, datetime.timezone.utc)
    # RFC 5322 format: Mon, 24 Nov 2025 14:00:00 +0000
    config.deb_date_rfc5322 = dt.strftime("%a, %d %b %Y %H:%M:%S %z")

    # Assuming SCRIPTDIR is provided in config and points to the debian
    # directory where changelog.template resides.
    script_dir = config.script_dir
    process_template(
        script_dir / "changelog.template",
        deb_changelog,
        config.get_template_context(),
    )

    run_command([
        "debchange",
        "-a",
        "--nomultimaint",
        "-m",
        "--changelog",
        str(deb_changelog),
        f"Release Notes: {config.releasenotes}",
    ])

    gzlog_dir = (
        config.staging_dir /
        f"usr/share/doc/{config.info_vars['PACKAGE']}-{config.channel}")
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


class InstallerTemplate(string.Template):
    delimiter = "@@"


def _process_template_includes(input_file: pathlib.Path,
                               include_stack: list[pathlib.Path]) -> str:
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


def process_template(input_file: pathlib.Path, output_file: pathlib.Path,
                     context: dict[str, str]) -> None:
    content = _process_template_includes(input_file, [])

    template = InstallerTemplate(content)
    try:
        content = template.substitute(context)
    except KeyError as e:
        print(
            f"Error: Var {e} was not found in context for {output_file}",
            file=sys.stderr,
        )
        sys.exit(1)
    except ValueError as e:
        print(f"Error: {e} in {output_file}", file=sys.stderr)
        sys.exit(1)

    with open(output_file, "w") as f:
        f.write(content)


@dataclasses.dataclass
class InstallerConfig:
    output_dir: pathlib.Path
    staging_dir: pathlib.Path
    channel: str
    branding: str
    arch: str
    target_os: str
    is_official_build: bool
    shlib_perms: int

    # From chromium-browser.info or google-chrome.info
    info_vars: dict[str, str] = dataclasses.field(default_factory=dict)

    # From version.txt
    version_vars: dict[str, str] = dataclasses.field(default_factory=dict)
    version: str = ""

    # From BRANDING
    branding_vars: dict[str, str] = dataclasses.field(default_factory=dict)

    # Computed
    package_release: str = "1"
    releasenotes: str = ""
    versionfull: str = ""
    package_orig: str = ""
    usr_bin_symlink_name: str = ""
    rdn_desktop: str = ""

    # Build script extras
    architecture: str = ""
    build_timestamp: str = ""
    script_dir: pathlib.Path = None
    tmp_file_dir: pathlib.Path = None
    common_deps: str = ""
    common_predeps: str = ""
    repoconfig: str = ""
    repoconfigregex: str = ""

    # Deb
    deb_pre_depends: str = ""
    deb_depends: str = ""
    deb_provides: str = ""
    deb_date_rfc5322: str = ""

    # RPM
    rpm_build_dir: pathlib.Path = None
    rpm_spec_file: pathlib.Path = None
    rpm_optional_man_page: str = ""
    rpm_package_filename: str = ""
    rpm_depends: str = ""
    rpm_provides: str = ""

    # Runtime/Installer
    logo_resources_png: str = ""
    uri_scheme: str = ""
    extra_desktop_entries: str = ""

    @classmethod
    def from_args(cls, args: argparse.Namespace,
                  output_dir: pathlib.Path) -> "InstallerConfig":
        data = cls._load_branding_and_version(output_dir, args.branding,
                                              args.channel)
        data.update({
            "arch": args.arch,
            "target_os": args.target_os,
            "architecture": args.arch,
            "branding": args.branding,
            "build_timestamp": args.build_time,
            "is_official_build": args.official,
            "output_dir": output_dir,
            "shlib_perms": StandardPermissions.EXECUTABLE,
            # Placeholder for build specific paths, set by caller if needed
            "script_dir": pathlib.Path("."),
            "staging_dir": pathlib.Path("."),
            "tmp_file_dir": pathlib.Path("."),
        })

        if hasattr(args, "sysroot"):
            # Debian specific
            (
                data["repoconfig"],
                data["repoconfigregex"],
            ) = cls._compute_deb_repoconfig(args.arch, data["package_orig"])
        else:
            # RPM specific
            (
                data["repoconfig"],
                data["repoconfigregex"],
            ) = cls._compute_rpm_repoconfig(data["package_orig"])

        config = cls(**data)
        config.rpm_package_filename = f"{config.package_orig}-{config.channel}"
        return config

    @staticmethod
    def _load_branding_and_version(output_dir: pathlib.Path, branding: str,
                                   channel: str) -> dict[str, typing.Any]:
        data = {}

        def parse_shell(file_path: pathlib.Path) -> dict[str, str]:
            env = {}
            with file_path.open("r") as f:
                for token in shlex.split(f.read()):
                    if "=" in token:
                        key, val = token.split("=", 1)
                        env[key] = val
            return env

        def parse_simple(file_path: pathlib.Path) -> dict[str, str]:
            return dict(
                line.strip().split("=") for line in file_path.open("r") if line)

        data["info_vars"] = parse_shell(
            output_dir / "installer/common" /
            ("google-chrome.info"
             if branding == "google_chrome" else "chromium-browser.info"))

        data["branding_vars"] = parse_simple(output_dir /
                                             "installer/theme/BRANDING")

        data["version_vars"] = parse_simple(output_dir / "installer" /
                                            "version.txt")
        data["version"] = (
            f"{data['version_vars']['MAJOR']}.{data['version_vars']['MINOR']}."
            f"{data['version_vars']['BUILD']}.{data['version_vars']['PATCH']}")
        data["package_release"] = "1"

        # Verify channel
        channel, data["releasenotes"] = normalize_channel(channel)
        data["channel"] = channel
        data["versionfull"] = f"{data['version']}-{data['package_release']}"
        data["package_orig"] = data["info_vars"]["PACKAGE"]
        data[
            "usr_bin_symlink_name"] = f"{data['info_vars']['PACKAGE']}-{channel}"
        if channel != "stable":
            data["info_vars"]["INSTALLDIR"] += f"-{channel}"
            data["info_vars"]["PACKAGE"] += f"-{channel}"
            data["info_vars"]["MENUNAME"] += f" ({channel})"
            data["rdn_desktop"] = f"{data['info_vars']['RDN']}.{channel}"
        else:
            data["rdn_desktop"] = data["info_vars"]["RDN"]

        return data

    @staticmethod
    def _compute_deb_repoconfig(arch: str, package: str) -> tuple[str, str]:
        base_repo_config = "dl.google.com/linux/chrome/deb/ stable main"
        default = f"deb [arch={arch}] https://{base_repo_config}"
        repoconfig = os.environ.get("REPOCONFIG", default)

        repo_config_regex = f"deb (\\[arch=[^]]*\\b{arch}\\b[^]]*\\]"
        repo_config_regex += f"[[:space:]]*) https?://{base_repo_config}"
        return repoconfig, repo_config_regex

    @staticmethod
    def _compute_rpm_repoconfig(package: str) -> tuple[str, str]:
        # ${PACKAGE#google-} removes "google-" prefix.
        if package.startswith("google-"):
            repo_package = package[7:]
        else:
            repo_package = package
        repoconfig = f"https://dl.google.com/linux/{repo_package}/rpm/stable"
        return repoconfig, ""

    def get_template_context(self) -> dict[str, str]:
        ctx = self.info_vars | self.version_vars | self.branding_vars
        for field in dataclasses.fields(self):
            val = getattr(self, field.name)
            if val is not None and type(val) is not dict:
                ctx[field.name] = val
        return ctx

    def get_binary_artifacts(self) -> list[Artifact]:
        progname = self.info_vars["PROGNAME"]
        artifacts = [
            Artifact(
                f"{progname}.stripped",
                progname,
                ArtifactType.BINARY,
                StandardPermissions.EXECUTABLE,
            ),
            Artifact(
                f"{progname}_sandbox.stripped",
                "chrome-sandbox",
                ArtifactType.BINARY,
                StandardPermissions.SANDBOX,
            ),
            Artifact(
                "chrome_crashpad_handler.stripped",
                "chrome_crashpad_handler",
                ArtifactType.BINARY,
                StandardPermissions.EXECUTABLE,
            ),
            Artifact(
                "chrome_management_service.stripped",
                "chrome-management-service",
                ArtifactType.BINARY,
                StandardPermissions.EXECUTABLE,
            ),
            Artifact(
                "lib/libc++.so",
                "lib/libc++.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                strip=True,
                is_optional=True,
            ),
            Artifact(
                "libEGL.so.stripped",
                "libEGL.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "libGLESv2.so.stripped",
                "libGLESv2.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "liboptimization_guide_internal.so.stripped",
                "liboptimization_guide_internal.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "libqt5_shim.so.stripped",
                "libqt5_shim.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "libqt6_shim.so.stripped",
                "libqt6_shim.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "libvk_swiftshader.so.stripped",
                "libvk_swiftshader.so",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "libvulkan.so.1.stripped",
                "libvulkan.so.1",
                ArtifactType.BINARY,
                self.shlib_perms,
                is_optional=True,
            ),
            Artifact(
                "vk_swiftshader_icd.json",
                "vk_swiftshader_icd.json",
                ArtifactType.BINARY,
                StandardPermissions.REGULAR,
                is_optional=True,
            ),
        ]

        # Widevine CDM
        if (self.output_dir / "WidevineCdm").is_dir():
            artifacts.append(
                Artifact(
                    "WidevineCdm",
                    "WidevineCdm",
                    ArtifactType.DIRECTORY,
                    StandardPermissions.EXECUTABLE,
                ))

        return artifacts

    def get_resource_artifacts(self) -> list[Artifact]:
        artifacts = [
            Artifact(
                "resources.pak",
                "resources.pak",
                ArtifactType.RESOURCE,
                StandardPermissions.REGULAR,
            ),
            Artifact(
                "icudtl.dat",
                "icudtl.dat",
                ArtifactType.RESOURCE,
                StandardPermissions.REGULAR,
            ),
        ]

        if (self.output_dir / "chrome_100_percent.pak").exists():
            artifacts.append(
                Artifact(
                    "chrome_100_percent.pak",
                    "chrome_100_percent.pak",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))
            artifacts.append(
                Artifact(
                    "chrome_200_percent.pak",
                    "chrome_200_percent.pak",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))
        else:
            artifacts.append(
                Artifact(
                    "theme_resources_100_percent.pak",
                    "theme_resources_100_percent.pak",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))
            artifacts.append(
                Artifact(
                    "ui_resources_100_percent.pak",
                    "ui_resources_100_percent.pak",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))

        if (self.output_dir / "v8_context_snapshot.bin").exists():
            artifacts.append(
                Artifact(
                    "v8_context_snapshot.bin",
                    "v8_context_snapshot.bin",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))
        else:
            artifacts.append(
                Artifact(
                    "snapshot_blob.bin",
                    "snapshot_blob.bin",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))

        # Default apps (Recursive)
        artifacts.append(
            Artifact(
                "default_apps",
                "default_apps",
                ArtifactType.DIRECTORY,
                is_optional=True,
            ))

        # Locales
        for pak in (self.output_dir / "locales").glob("*.pak"):
            artifacts.append(
                Artifact(
                    pak.relative_to(self.output_dir),
                    pathlib.Path("locales") / pak.name,
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))

        # Privacy Sandbox Attestation
        psa_manifest = (
            self.output_dir /
            "PrivacySandboxAttestationsPreloaded/manifest.json")
        if psa_manifest.exists():
            artifacts.append(
                Artifact(
                    "PrivacySandboxAttestationsPreloaded/manifest.json",
                    "PrivacySandboxAttestationsPreloaded/manifest.json",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))
            artifacts.append(
                Artifact(
                    "PrivacySandboxAttestationsPreloaded/privacy-sandbox-attestations.dat",
                    "PrivacySandboxAttestationsPreloaded/privacy-sandbox-attestations.dat",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))

        # MEI Preload
        mei_manifest = self.output_dir / "MEIPreload/manifest.json"
        if mei_manifest.exists():
            artifacts.append(
                Artifact(
                    "MEIPreload/manifest.json",
                    "MEIPreload/manifest.json",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))
            artifacts.append(
                Artifact(
                    "MEIPreload/preloaded_data.pb",
                    "MEIPreload/preloaded_data.pb",
                    ArtifactType.RESOURCE,
                    StandardPermissions.REGULAR,
                ))

        return artifacts

    def get_icon_artifacts(self) -> list[Artifact]:
        artifacts = []
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
            artifacts.append(
                Artifact(
                    pathlib.Path("installer/theme") / logo,
                    logo,
                    ArtifactType.ICON,
                    StandardPermissions.REGULAR,
                ))
        self.logo_resources_png = " " + " ".join(logo_resources_png)
        return artifacts

    def get_desktop_integration_artifacts(self) -> list[Artifact]:
        artifacts = []

        # CHROME_VERSION_EXTRA
        if self.branding == "google_chrome":
            artifacts.append(
                Artifact(
                    "",
                    "CHROME_VERSION_EXTRA",
                    ArtifactType.GENERATED,
                    StandardPermissions.REGULAR,
                    content=self.channel + "\n",
                ))

        # wrapper script
        artifacts.append(
            Artifact(
                "installer/common/wrapper",
                self.info_vars["PACKAGE"],
                ArtifactType.TEMPLATE,
                StandardPermissions.EXECUTABLE,
            ))

        # symlink for PACKAGE_ORIG
        package_orig = self.package_orig
        if package_orig and package_orig != self.info_vars["PACKAGE"]:
            link_target = os.path.join(
                self.info_vars["INSTALLDIR"],
                self.info_vars["PACKAGE"],
            )
            artifacts.append(
                Artifact(
                    "",
                    package_orig,
                    ArtifactType.SYMLINK,
                    dst_base="install_dir",
                    symlink_target=link_target,
                ))

        # /usr/bin symlink
        if self.usr_bin_symlink_name:
            link_target = os.path.join(
                self.info_vars["INSTALLDIR"],
                self.info_vars["PACKAGE"],
            )
            artifacts.append(
                Artifact(
                    "",
                    pathlib.Path("usr/bin") / self.usr_bin_symlink_name,
                    ArtifactType.SYMLINK,
                    dst_base="staging_dir",
                    symlink_target=link_target,
                ))

        # URI_SCHEME logic
        if self.branding == "google_chrome":
            if self.channel not in ["beta", "unstable", "canary"]:
                self.uri_scheme = "x-scheme-handler/google-chrome;"
        else:
            self.uri_scheme = "x-scheme-handler/chromium;"

        # xdg-mime and xdg-settings
        artifacts.append(
            Artifact(
                "xdg-mime",
                "xdg-mime",
                ArtifactType.RESOURCE,
                StandardPermissions.EXECUTABLE,
            ))
        artifacts.append(
            Artifact(
                "xdg-settings",
                "xdg-settings",
                ArtifactType.RESOURCE,
                StandardPermissions.EXECUTABLE,
            ))

        # appdata.xml
        artifacts.append(
            Artifact(
                "installer/common/appdata.xml.template",
                pathlib.Path("usr/share/appdata") /
                f"{self.info_vars['PACKAGE']}.appdata.xml",
                ArtifactType.TEMPLATE,
                StandardPermissions.REGULAR,
                dst_base="staging_dir",
            ))

        # desktop file
        self.extra_desktop_entries = ""
        artifacts.append(
            Artifact(
                "installer/common/desktop.template",
                pathlib.Path("usr/share/applications") /
                f"{self.info_vars['PACKAGE']}.desktop",
                ArtifactType.TEMPLATE,
                StandardPermissions.REGULAR,
                dst_base="staging_dir",
            ))

        # rdn desktop file
        rdn_extra_desktop_entries = (
            f"# This is the same as {self.info_vars['PACKAGE']}.desktop except "
            "NoDisplay=true prevents\n"
            "# duplicate menu entries. This is required to match "
            "the application ID\n"
            "# used by XDG desktop portal, which has stricter "
            "naming requirements.\n"
            "# The old desktop file is kept to preserve default "
            "browser settings.\n"
            "NoDisplay=true\n")

        artifacts.append(
            Artifact(
                "installer/common/desktop.template",
                pathlib.Path("usr/share/applications") /
                f"{self.rdn_desktop}.desktop",
                ArtifactType.TEMPLATE,
                StandardPermissions.REGULAR,
                dst_base="staging_dir",
                template_context={
                    "extra_desktop_entries": rdn_extra_desktop_entries
                },
            ))

        # default-apps
        artifacts.append(
            Artifact(
                "installer/common/default-app.template",
                pathlib.Path("usr/share/gnome-control-center/default-apps") /
                f"{self.info_vars['PACKAGE']}.xml",
                ArtifactType.TEMPLATE,
                StandardPermissions.REGULAR,
                dst_base="staging_dir",
            ))

        # default-app-block
        artifacts.append(
            Artifact(
                "installer/common/default-app-block.template",
                "default-app-block",
                ArtifactType.TEMPLATE,
                StandardPermissions.REGULAR,
            ))

        # documentation (manpage)
        man_page_dst = (
            pathlib.Path("usr/share/man/man1") /
            f"{self.usr_bin_symlink_name}.1")
        artifacts.append(
            Artifact(
                "installer/common/manpage.1.in",
                man_page_dst,
                ArtifactType.TEMPLATE,
                StandardPermissions.REGULAR,
                dst_base="staging_dir",
                compress=True,
            ))

        # Link for stable channel app-without-channel case
        if self.info_vars["PACKAGE"] != self.usr_bin_symlink_name:
            artifacts.append(
                Artifact(
                    "",
                    pathlib.Path("usr/share/man/man1") /
                    f"{self.info_vars['PACKAGE']}.1.gz",
                    ArtifactType.SYMLINK,
                    dst_base="staging_dir",
                    symlink_target=f"{self.usr_bin_symlink_name}.1.gz",
                    is_optional=True,
                ))

        return artifacts

    def get_artifacts(self) -> list[Artifact]:
        return (self.get_binary_artifacts() + self.get_resource_artifacts() +
                self.get_icon_artifacts() +
                self.get_desktop_integration_artifacts())


class Installer:

    def __init__(self, config: InstallerConfig) -> None:
        self.config = config

    def prep_staging_common(self) -> None:
        install_dir = self.config.staging_dir / self.config.info_vars[
            "INSTALLDIR"].lstrip("/")
        dirs = [
            install_dir,
            self.config.staging_dir / "usr/bin",
            self.config.staging_dir / "usr/share/applications",
            self.config.staging_dir / "usr/share/appdata",
            (self.config.staging_dir /
             "usr/share/gnome-control-center/default-apps"),
            self.config.staging_dir / "usr/share/man/man1",
        ]
        for d in dirs:
            d.mkdir(parents=True, exist_ok=True)
            d.chmod(StandardPermissions.EXECUTABLE)

    def stage_install_common(self) -> None:
        logging.info(
            f"Staging common install files in '{self.config.staging_dir}'...")
        install_dir = self.config.staging_dir / self.config.info_vars[
            "INSTALLDIR"].lstrip("/")

        artifacts = self.config.get_artifacts()
        self._process_artifacts(artifacts, install_dir)

        # Verify ELF binaries
        self._verify_elf_binaries(install_dir)
        self._verify_file_permissions()

    def _process_artifacts(self, artifacts: list[Artifact],
                           install_dir: pathlib.Path) -> None:
        for artifact in artifacts:
            # Determine destination
            dst = install_dir / artifact.dst
            if artifact.dst_base == "staging_dir":
                dst = self.config.staging_dir / artifact.dst

            # Determine source (if applicable)
            src = None
            if artifact.src:
                src = self.config.output_dir / artifact.src
                if not src.exists():
                    if artifact.is_optional:
                        continue
                    raise FileNotFoundError(
                        f"Required artifact not found: {src}")
            elif not artifact.is_optional and artifact.artifact_type not in (
                    ArtifactType.GENERATED,
                    ArtifactType.SYMLINK,
            ):
                # Only GENERATED and SYMLINK are allowed to have no src
                # (unless is_optional is True)
                raise ValueError(f"Artifact {artifact.dst} missing source")

            if artifact.artifact_type == ArtifactType.DIRECTORY:
                if dst.exists():
                    shutil.rmtree(dst)
                shutil.copytree(src, dst)
                # Apply permissions
                for root, dirs, files in os.walk(dst):
                    os.chmod(root, StandardPermissions.EXECUTABLE)
                    for d in dirs:
                        os.chmod(
                            os.path.join(root, d),
                            StandardPermissions.EXECUTABLE,
                        )
                    for f in files:
                        # Heuristic for shared libraries inside directories
                        # (like Widevine)
                        path = os.path.join(root, f)
                        if f.endswith(".so") or ".so." in f:
                            os.chmod(path, self.config.shlib_perms)
                        else:
                            os.chmod(path, StandardPermissions.REGULAR)

            elif artifact.artifact_type == ArtifactType.TEMPLATE:
                dst.parent.mkdir(parents=True, exist_ok=True)
                context = self.config.get_template_context()
                if artifact.template_context:
                    context.update(artifact.template_context)
                process_template(src, dst, context)
                if artifact.compress:
                    run_command(["gzip", "-9nf", str(dst)])
                    dst = dst.parent / (dst.name + ".gz")
                dst.chmod(artifact.mode)

            elif artifact.artifact_type == ArtifactType.GENERATED:
                dst.parent.mkdir(parents=True, exist_ok=True)
                with dst.open("w") as f:
                    f.write(artifact.content)
                dst.chmod(artifact.mode)

            elif artifact.artifact_type == ArtifactType.SYMLINK:
                if dst.is_symlink() or dst.exists():
                    dst.unlink()
                # Create parent if needed
                dst.parent.mkdir(parents=True, exist_ok=True)
                os.symlink(artifact.symlink_target, dst)

            elif artifact.artifact_type == ArtifactType.ICON:
                self._install(src, dst, artifact.mode)

            elif artifact.artifact_type in (
                    ArtifactType.BINARY,
                    ArtifactType.RESOURCE,
            ):
                self._install(src, dst, artifact.mode, artifact.strip)

            else:
                raise ValueError(
                    f"Unknown artifact type: {artifact.artifact_type}")

    def _install_into_dir(
        self,
        src: pathlib.Path,
        dest_dir: pathlib.Path,
        mode: int | None = None,
        strip: bool = False,
    ) -> None:
        dest = dest_dir / src.name
        self._install(src, dest, mode, strip)

    def _install(
        self,
        src: pathlib.Path,
        dest: pathlib.Path,
        mode: int | None = None,
        strip: bool = False,
    ) -> None:
        dest.parent.mkdir(parents=True, exist_ok=True)

        shutil.copy(src, dest)
        if strip:
            run_command(["strip", str(dest)])
        if mode is not None:
            dest.chmod(mode)

    def _verify_elf_binaries(self, install_dir: pathlib.Path) -> None:
        unstripped = []
        rpath_bins = []
        elf_outside = []

        for root, _, files in os.walk(self.config.staging_dir):
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

                    if self.config.target_os != "chromeos":
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

    def _verify_file_permissions(self) -> None:
        # Verify file permissions
        on_cog = False
        if not self.config.is_official_build:
            # On Cog, permission is always 0664 or 0775
            if os.getcwd().startswith("/google/cog/cloud/"):
                on_cog = True
                print(
                    "INFO: build on Cog. relax permission for group writable",
                    file=sys.stderr,
                )

        for root, dirs, files in os.walk(self.config.staging_dir):
            # Check directories
            for d in dirs:
                pass

            # We iterate everything.
            for item in dirs + files:
                path = pathlib.Path(root) / item
                st = path.lstat()  # lstat to not follow symlinks
                actual_perms = stat.S_IMODE(st.st_mode)

                base_name = item
                expected_perms = StandardPermissions.REGULAR  # Default

                if path.is_dir():
                    expected_perms = StandardPermissions.EXECUTABLE
                elif path.is_symlink():
                    target = os.readlink(path)
                    if target.startswith("/"):
                        expect_exists = self.config.staging_dir / target.lstrip(
                            "/")
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
                    expected_perms = StandardPermissions.EXECUTABLE
                elif base_name == "chrome-sandbox":
                    expected_perms = StandardPermissions.SANDBOX
                elif "shell script" in file_type:
                    expected_perms = StandardPermissions.EXECUTABLE
                elif "ELF" in file_type:
                    if base_name.endswith(".so") or ".so." in base_name:
                        expected_perms = self.config.shlib_perms
                    else:
                        expected_perms = StandardPermissions.EXECUTABLE

                if expected_perms != actual_perms:
                    ok = False
                    relaxed_expected_perms = expected_perms
                    if on_cog:
                        if expected_perms == StandardPermissions.SANDBOX:
                            relaxed_expected_perms = 0o775
                        elif expected_perms == StandardPermissions.REGULAR:
                            relaxed_expected_perms = 0o664
                        elif expected_perms == StandardPermissions.EXECUTABLE:
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
