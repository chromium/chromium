# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import pathlib
import re
import shutil
import struct
import sys
import hashlib
import subprocess
import tempfile
import typing
from typing import Literal, NewType, TypeAlias, Iterable, NamedTuple

CompressionType: TypeAlias = Literal[b"bzip2", b"zlib", b"lzma"]

CString = NewType("CString", bytes)
"""A byte sequence that will be provided to a C program. Must be NUL-free."""


def CStr(sb: str | bytes) -> CString:
    """Converts a `str` or `bytes` to a CString if safe.

    A `str` is safe if it can be encoded as ASCII and contains no NUL bytes.
    A `bytes` is safe if it contains no NUL bytes.

    Args:
        sb: `str` or `bytes` to check and convert.

    Returns:
        The input as `bytes`, tagged `CString` for type analysis purposes.

    Raises:
        UnicodeError: `sb` was `str` and contained non-ASCII characters.
        ValueError: `sb` contained NUL.
    """

    match sb:
        case bytes(mb):
            b = mb
        case str(ms):
            b = ms.encode("ascii")
        case _ as unreachable:
            typing.assert_never(unreachable)
            b = b""  # Make Pyright stop fearing an unbound `b`
    for c in b:
        if c == 0:
            raise ValueError(
                f"C-safe string cannot contain NUL; arg was '{sb!a}'")
    return CString(b)


InteriorPath = NewType("InteriorPath", CString)
"""A file path referring to the virtual file system inside a disk image."""


def DmgPath(sb: str) -> InteriorPath:
    """Converts a string to an InteriorPath. Checks C-string safety."""
    return InteriorPath(CStr(sb))


def _itoa(i: int) -> bytes:
    return str(i).encode("ascii")


VerifiedItemType: TypeAlias = Literal["file", "symlink", "dir", "bizarre"]


class Expectation(NamedTuple):
    type: VerifiedItemType
    checksum: bytes | None


class TreeVerifier:
    """Verifies that a directory tree matches a pre-calculated state."""

    def __init__(self, root: pathlib.Path):
        """Calculates checksums for all files in root."""
        self._root = root
        self._checksums: dict[pathlib.Path, Expectation] = {}
        for p in root.rglob('*'):
            self._checksums[p.relative_to(root)] = self._calculate_expectation(
                p)

    def _calculate_expectation(self, path: pathlib.Path) -> Expectation:
        # Check for symlinks first, since `is_file` and `is_dir` will follow
        # a terminal symlink instead of saying "no, that's a symlink". Their
        # `follow_symlinks` parameters were not introduced until Python 3.13.
        if path.is_symlink():
            return Expectation("symlink", self._sha256_symlink_target(path))
        if path.is_file():
            return Expectation("file", self._sha256_file(path))
        if path.is_dir():
            return Expectation("dir", None)
        raise ValueError(f"Path is not a file, symlink, or directory: {path}")

    def _sha256_symlink_target(self, path: pathlib.Path) -> bytes:
        target = os.readlink(path)
        return hashlib.sha256(os.fsencode(target)).digest()

    def _sha256_file(self, path: pathlib.Path) -> bytes:
        h = hashlib.sha256()
        n = 0
        try:
            with open(path, "rb") as f:
                while chunk := f.read(8192):
                    n += len(chunk)
                    h.update(chunk)
            return h.digest()
        except Exception as e:
            e.add_note(f"Failed to read {path} after {n} bytes")
            raise

    def verify(self, target_root: pathlib.Path):
        """Verifies target_root matches the pre-calculated checksums.

        Args:
            target_root: Path to the root of the tree to verify.

        Raises:
            ValueError: If the trees do not match.
        """
        # 1. Check all expected files exist and match
        for rel_path, want in self._checksums.items():
            target_path = target_root / rel_path
            if not target_path.exists():
                raise ValueError(f"Missing file: {rel_path}")

            got = self._calculate_expectation(target_path)

            if got != want:
                if got.type != want.type:
                    raise ValueError(f"Target type mismatch for {rel_path}: "
                                     f"want {want.type}, got {got.type}")
                raise ValueError(f"Checksum mismatch for {rel_path}")

        # 2. Check for unexpected files in target
        for p in target_root.rglob('*'):
            rel_path = p.relative_to(target_root)

            if rel_path not in self._checksums:
                raise ValueError(f"Unexpected file in target: {rel_path}")


class HfsTool:
    """Wraps the `hfs_tool` binary for manipulating HFS+ disk images."""

    def __init__(self, p: pathlib.Path):
        self._tool_path = os.fsencode(p)

    def addall(self,
               target_udif_path: pathlib.Path,
               src_dir: pathlib.Path,
               dest_dir_in_dmg: InteriorPath = DmgPath("/")):
        """Invokes `hfs_tool addall` to copy into an HFS+-formatted raw UDIF.

        Symlinks are duplicated, not traversed. File modes are preserved.

        Args:
            target_udif_path: Where to find the existing HFS+-formatted raw
              UDIF with no volume map, to copy files into. It must have enough
              empty space for the specified files.
            src_dir: Directory to copy out of.
            dest_dir_in_dmg: Path to copy all files from src_dir into.

        Raises:
            subprocess.CalledProcessError: `hfs_tool` is sad. Maybe stdout will
              help explain why.
        """
        subprocess.check_call([
            self._tool_path,
            os.fsencode(target_udif_path), b"addall", b"--symlinks",
            b"clone_link", b"--special-modes", b"no",
            os.fsencode(src_dir),
            bytes(dest_dir_in_dmg)
        ])

    def set_omaha_tag(self, target_udif_path: pathlib.Path,
                      target_inside_dmg: InteriorPath, tag_value: CString):
        """Invokes `hfs_tool setattr` to apply an Omaha tag.

        Args:
            target_udif_path: Where to find the existing HFS+-formatted
              raw UDIF with no volume map, which already contains the
              item to which the attribution should be attached. It must
              have enough empty space for the attribution.
            target_inside_dmg: Path to the item (inside the virtual
              HFS+ volume) to attach an extended attribute to.
            tag_value: Content of the Omaha tag.
        """
        if len(tag_value) > 8192:
            raise ValueError(f"Omaha tag too long ({len(tag_value)} bytes)")

        subprocess.check_call([
            self._tool_path,
            os.fsencode(target_udif_path), b"setattr", b"--data-format",
            b"omaha-tag-zone",
            bytes(target_inside_dmg), b"com.apple.application-instance",
            bytes(tag_value)
        ])

    def enable_folder_icon(self, target_udif_path: pathlib.Path):
        """Sets the "custom folder icon" HFS+ flag on the DMG root.

        Args:
            target_udif_path: Where to find the existing HFS+-formatted
              raw UDIF with no volume map.
        """
        subprocess.check_call([
            self._tool_path,
            os.fsencode(target_udif_path), b"attr", b"/", b"C"
        ])


class DmgTool:
    """Wraps the `dmg_tool` binary for building and modifying DMG files."""

    def __init__(self, p: pathlib.Path):
        self._tool_path = os.fsencode(p)

    def build(self,
              udif_in_path: pathlib.Path,
              dmg_out_path: pathlib.Path,
              tag_placeholder: CString | None,
              run_sectors: int = 8192,
              compression: CompressionType | None = None,
              level: int | None = None):
        """Invokes `dmg_tool build` to convert a raw UDIF to a DMG.

        Args:
            udif_in_path: Where to find the existing raw UDIF to convert.
            dmg_out_path: Where to save the generated DMG.
            tag_placeholder: Do not compress the chunk(s) containing a complete
              Omaha tag with this tag content, storing partial checksums in
              the DMG's resources, so it can be retagged later.
            run_sectors: Number of sectors per chunk. Higher values compress
              better, require more RAM and time to decompress, and result in
              larger uncompressed segments when retagging.
            compression: Algorithm for compressing the DMG. See CompressionType
              for supported compression algorithms. If None, do not compress.
            level: Compression level parameter for the compression algorithm.
              Interpretation is compressor-specific. If None, use default,
              which is also compressor-specific.
        """
        args = [
            self._tool_path, b"--run-sectors",
            _itoa(run_sectors), b"--data-format", b"omaha-tag-zone"
        ]
        if compression is not None:
            args += [b"--compression", compression]
        if level is not None:
            args += [b"--level", _itoa(level)]
        args += [b"build", os.fsencode(udif_in_path), os.fsencode(dmg_out_path)]
        if tag_placeholder is not None:
            args.append(tag_placeholder)

        subprocess.check_call(args)

    def attribute(self, dmg_in_path: pathlib.Path, dmg_out_path: pathlib.Path,
                  tag_placeholder: CString, new_tag: CString):
        """Invokes `dmg_tool attribute` to re-tag a DMG created via `build`.

        Args:
            dmg_in_path: Where to find the existing DMG to re-tag.
            dmg_out_path: Where to save the re-tagged copy.
            tag_placeholder: Placeholder Omaha tag data already embedded in the
              file at dmg_in_path in a full-size Omaha tag zone.
            new_tag: Tag data to set, overwriting the placeholder. The result
              will be a full-length valid Omaha tag.

        Raises:
            ValueError: `new_tag` is too long.
            subprocess.CalledProcessError: dmg_tool returned an error. Check
              stdout and stderr for hints. "cannot find file" and "cannot find
              placeholder" are the most likely issues.
        """
        if len(new_tag) > 8192:
            raise ValueError(f"Omaha tag too long ({len(new_tag)} bytes)")

        subprocess.check_call([
            self._tool_path, b"--data-format", b"omaha-tag-zone", b"attribute",
            os.fsencode(dmg_in_path),
            os.fsencode(dmg_out_path),
            bytes(tag_placeholder),
            bytes(new_tag)
        ])


class HdiUtil:
    """Wraps the `hdiutil` system utility (Apple's disk image tool)."""

    def __init__(self, p: pathlib.Path | None = None):
        if not p:
            found = shutil.which("hdiutil")
            if not found:
                raise RuntimeError("hdiutil not found")
            p = pathlib.Path(found)
        self._tool_path = os.fsencode(p)

    @contextlib.contextmanager
    def scoped_attach(self, dmg_path: pathlib.Path):
        """Mounts a DMG at a temporary location and yields the mount point.

        Mounts with -nobrowse, -readonly, -noverify to minimize interference.
        Unmounts (detaches) the DMG when the context manager exits.

        Args:
            dmg_path: Path to the DMG to mount.

        Yields:
            pathlib.Path: The path to the mount point.
        """
        mount_root = pathlib.Path(tempfile.mkdtemp(prefix="rebranding_"))
        try:
            # We use a randomized mount point to avoid collisions and to ensure
            # we know exactly where it is.
            subprocess.check_call([
                self._tool_path,
                "attach",
                dmg_path,
                "-mountpoint",
                mount_root,
                "-nobrowse",
                "-readonly",
            ])
            yield mount_root
        finally:
            # -force is used to ensure we clean up even if something is holding
            # the volume open (though we should avoid that).
            subprocess.call([self._tool_path, "detach", mount_root, "-force"],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
            try:
                os.rmdir(mount_root)
            except OSError:
                pass

    def create_blank_taggable(self, dmg_output_path: pathlib.Path,
                              size_bytes: int, volume_name: str):
        """Creates a blank, taggable HFS+ UDIF image.

        The image is created with specific settings to be compatible with
        `hfs_tool` and `dmg_tool`'s tagging requirements (no partition map,
        16k attribute nodes).

        Args:
            dmg_output_path: Path to write the new UDIF to.
            size_bytes: Size of the filesystem in bytes. Will be rounded up
                to the nearest KiB.
            volume_name: Name of the volume.
        """

        # The UDIF created here has several weird traits that are mandatory
        # for compatibility with `hfs_tool` and Chrome tagging:
        # - There is no partition map, so it is a single logical device; this
        #   is the only `-layout` supported by `hfs_tool`
        # - The nodes in the "attributes" B-tree are 16k instead of the
        #   default 8k; this allows a complete Omaha tag to fit in an inline
        #   attribute node, the only way `hfs_tool` writes attributes.
        # - The size is rounded up to a kilobyte increment because
        #   hdiutil doesn't accept a size in bytes. (Specifying a size with a
        #   `b` suffix gets sectors rather than bytes.)
        subprocess.check_call([
            self._tool_path, "create", "-type", "UDIF", "-size",
            str((size_bytes + 1023) // 1024) + "k", "-volname", volume_name,
            "-fs", "HFS+", "-layout", "NONE", "-fsargs", "-n a=16384",
            dmg_output_path
        ])

        # hdiutil might append .dmg if it's missing, rename it back if so.
        if not dmg_output_path.exists():
            potential_path = dmg_output_path.with_name(dmg_output_path.name +
                                                       ".dmg")
            if potential_path.exists():
                potential_path.rename(dmg_output_path)
            else:
                raise FileNotFoundError(
                    f"Could not find created DMG at {dmg_output_path} or "
                    f"{potential_path}")


def create_taggable_dmg(hfs_tool: HfsTool, dmg_tool: DmgTool, hdiutil: HdiUtil,
                        original_dmg: pathlib.Path,
                        output_dmg_path: pathlib.Path,
                        scratch_dir: pathlib.Path, volume_name: str,
                        app_name: str):
    """Creates a blank taggable DMG and populates it from an original DMG.

    Args:
        hfs_tool: Tool buffer.
        dmg_tool: Tool buffer.
        hdiutil: Tool buffer.
        original_dmg: Source DMG to copy files from.
        output_dmg_path: Where to write the final compressed taggable DMG.
        scratch_dir: Directory for temporary files.
        volume_name: Name of the volume in the new DMG.
        app_name: Name of the .app bundle to tag (e.g. "Google Chrome.app").
    """
    udif_path = scratch_dir / "temp.udif.dmg"
    with hdiutil.scoped_attach(original_dmg) as mount_path:
        # Calculate total size of files, rounding up to alloc blocks
        total_size = sum((f.lstat().st_size + 4095) // 4096
                         for f in mount_path.rglob('*')) * 4096

        # dmg_tool build corrupts the file system if it doesn't have
        # plenty of slack space. Cause and exact quantity are not yet known.
        # TODO(crbug.com/487428873): Investigate and fix this `dmg_tool build`
        #   data corruption bug.
        params_size = int(total_size * 2) + (64 * 1024 * 1024)

        hdiutil.create_blank_taggable(udif_path, params_size, volume_name)

        hfs_tool.addall(udif_path, mount_path)
        hfs_tool.enable_folder_icon(udif_path)

        # Omaha tags set by this script always occupy the maximum length of
        # a tag, since they reserve space for subsequent re-tagging. The
        # zero-length string specifies a full-length empty tag (the empty
        # string is valid tag data).
        hfs_tool.set_omaha_tag(udif_path, DmgPath(f"/{app_name}"), CString(b""))

        dmg_tool.build(udif_path, output_dmg_path, CString(b""), 8192, b"lzma",
                       9)
        os.remove(udif_path)


def deep_verify(verifier: TreeVerifier, test_dmg: pathlib.Path,
                hdiutil: HdiUtil, app_name: str, expected_tag: bytes) -> None:
    """Verifies that a DMG matches the source tree and has the correct tag.

    Args:
        verifier: TreeVerifier initialized with the source tree.
        test_dmg: Branded DMG to verify.
        hdiutil: Wrapper for accessing hdiutil.
        app_name: Name of the app to check tag on.
        expected_tag: Expected Omaha tag string (bytes).

    Raises:
        ValueError: If contents mismatch or tag is incorrect.
    """
    with hdiutil.scoped_attach(test_dmg) as dmg_mount_path:
        # 1. Verify file content matches source
        verifier.verify(dmg_mount_path)

        # 2. Check tag on dmg_b
        app_path_b = dmg_mount_path / app_name
        if not app_path_b.exists():
            raise ValueError(f"{app_name} not found in {test_dmg}")

        try:
            # Use macOS `xattr` tool to read value (binary) via stdout. Use
            # hex format since xattr does not like emitting NUL bytes in ASCII
            # format, and we want to check the entire tag zone.
            raw_output = subprocess.check_output([
                "xattr", "-p", "-x", "com.apple.application-instance",
                app_path_b
            ],
                                                 stderr=subprocess.PIPE)
            output = bytes.fromhex(raw_output.decode("ascii"))
            # See chrome/updater/tag.h for tag format details.
            magic = b"Gact2.0Omaha"
            if not output.startswith(magic):
                raise ValueError("Invalid magic in tag")

            # Parse length (big-endian uint16)
            if len(output) < 14:
                raise ValueError("Tag too short")
            tag_len = struct.unpack(">H", output[12:14])[0]
            if tag_len > 8192:
                raise ValueError(
                    f"Tag longer than Omaha tag data limit (8192): {tag_len}")
            tag_content = output[14:14 + tag_len]

            if tag_content != expected_tag:
                raise ValueError(
                    f"Tag mismatch. Expected {expected_tag}, got {tag_content}")

            tag_remainder = output[14 + tag_len:]
            if len(tag_remainder) != 8192 - tag_len:
                raise ValueError(
                    f"Wrong padding length. Expected {8192 - tag_len}, "
                    f"got {len(tag_remainder)}")
            if tag_remainder != b"\0" * (8192 - tag_len):
                raise ValueError("Nonzero bytes found in tag remainder")

        except subprocess.CalledProcessError:
            # If xattr missing, it's an error for branded dmg
            raise ValueError("Failed to read Omaha tag xattr")


def rebrand_chromium_dmg(hfs_tool: HfsTool, dmg_tool: DmgTool,
                         source_dmg_path: pathlib.Path,
                         output_dir: pathlib.Path, brand_codes: Iterable[str],
                         app_name: str, version: str, channel: str) -> None:
    """Takes a Chromium DMG, creates a blank taggable version, and brands it.

    Args:
        hfs_tool: Wrapper for hfs_tool.
        dmg_tool: Wrapper for dmg_tool.
        source_dmg_path: Path to the input DMG that will be rebranded.
        output_dir: Directory to place the branded DMGs.
        brand_codes: List of brand codes (e.g. ["GGLS", "EUBW"]).
        app_name: The name of the app without the `.app` extension, which is
            also used as the volume name.
        version: The version of the app.
        channel: The channel of the app.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    squished_app_name = re.sub(r"\s", "", app_name)
    base_filename = f"{squished_app_name}-{version}-{channel}"

    with tempfile.TemporaryDirectory(prefix="rebrand_scratch_") as scratch:
        hdiutil = HdiUtil()
        scratch_dir = pathlib.Path(scratch)
        base_taggable = output_dir / f"{base_filename}-BlankTag.dmg"

        print("Creating base taggable/template DMG...")
        create_taggable_dmg(hfs_tool, dmg_tool, hdiutil, source_dmg_path,
                            base_taggable, scratch_dir, app_name,
                            app_name + ".app")

        # Verify the blank tag
        print("Calculating checksums of source DMG...")
        with hdiutil.scoped_attach(source_dmg_path) as source_mount:
            verifier = TreeVerifier(source_mount)

            # Verify the blank tag
            print("Verifying base template...")
            deep_verify(verifier, base_taggable, hdiutil, app_name + ".app",
                        b"")

            for brand in brand_codes:
                print(f"Branding {brand}...")
                branded_dmg_path = (
                    output_dir / f"{base_filename}-Brand-{brand}.dmg")

                brand_tag = f"brand={brand}".encode('ascii')

                dmg_tool.attribute(base_taggable, branded_dmg_path,
                                   CString(b""), CString(brand_tag))

                # Verify
                deep_verify(verifier, branded_dmg_path, hdiutil,
                            app_name + ".app", brand_tag)
                print(f"Created {branded_dmg_path}")
