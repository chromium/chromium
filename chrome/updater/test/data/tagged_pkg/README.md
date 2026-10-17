# Tagged PKG Test Fixtures

This directory contains test PKG files used by `PkgTagTest` in
`tag_unittest.cc` and `KSAdminRegisterWithTaggedPkg` in
`test/integration_tests.cc`.

## Fixtures

* `sample_no_notarization_trailer_untagged.pkg`:
  A test untagged PKG file without a notarization trailer.
* `sample_no_notarization_trailer_empty_tag.pkg`:
  The untagged PKG with an empty tag space injected and padded to the default
  tag area size (8206 bytes, `kDefaultBinaryTagBytes`).
* `sample_no_notarization_trailer_brand_tagged.pkg`:
  The untagged PKG tagged with `brand=GGLZ` and `--padded-length=0` (shrunk to
  fit the exact tag content length).
* `sample_with_notarization_trailer_untagged.pkg`:
  The base untagged PKG with an appended synthetic 32-byte notarization trailer
  (two 16-byte records with `'t8lr'` magic `0x74, 0x38, 0x6c, 0x72`). Note:
  this is a mock trailer for unit test verification, not a real stapled Apple
  ticket.
* `sample_with_notarization_trailer_empty_tag.pkg`:
  The trailer PKG with an empty tag space injected before the trailer, padded to
  the default 8206 bytes.
* `sample_with_notarization_trailer_brand_tagged.pkg`:
  The trailer PKG tagged with `brand=GGLZ` and `--padded-length=0`, inserted
  before the trailer.

## How to recreate the fixtures

To create the fixtures, you need `pkgbuild`, `tag_exe` (built in `out/Default`),
and a Mac.

1. **Create the base test PKG**:
   ```bash
   pkgbuild --identifier com.google.Chrome.Test --version 1.0.0.0 \
       --nopayload sample_no_notarization_trailer_untagged.pkg
   ```

2. **Create the untagged fixture with fake notarization trailer**:
   Append a 32-byte mock trailer (containing two 16-byte `t8lr` records):
   ```bash
   cp sample_no_notarization_trailer_untagged.pkg \
       sample_with_notarization_trailer_untagged.pkg
   printf '\x74\x38\x6c\x72\x01\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00' \
       >> sample_with_notarization_trailer_untagged.pkg
   printf '\x74\x38\x6c\x72\x01\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00' \
       >> sample_with_notarization_trailer_untagged.pkg
   ```

3. **Generate empty-tagged fixtures** (default padding to 8206 bytes):
   ```bash
   out/Default/tag_exe --set-tag \
       --out=sample_no_notarization_trailer_empty_tag.pkg \
       sample_no_notarization_trailer_untagged.pkg

   out/Default/tag_exe --set-tag \
       --out=sample_with_notarization_trailer_empty_tag.pkg \
       sample_with_notarization_trailer_untagged.pkg
   ```

4. **Generate brand-tagged fixtures** (shrunk to exact tag length):
   ```bash
   out/Default/tag_exe --set-tag="brand=GGLZ" --padded-length=0 \
       --out=sample_no_notarization_trailer_brand_tagged.pkg \
       sample_no_notarization_trailer_untagged.pkg

   out/Default/tag_exe --set-tag="brand=GGLZ" --padded-length=0 \
       --out=sample_with_notarization_trailer_brand_tagged.pkg \
       sample_with_notarization_trailer_untagged.pkg
   ```

The `tag_exe` tool detects whether a notarization trailer is present and inserts
or overwrites the tag space immediately preceding the trailer, preserving the
trailer at the end of the file.
