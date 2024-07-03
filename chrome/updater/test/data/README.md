# selfupdate\_test\_key.der
A DER-formatted PKCS #8 PrivateKeyInfo for an RSA key used to sign the self-
update CRX used in integration tests. You can regenerate it by running:

```
openssl genrsa 4096 | openssl pkcs8 -inform PEM -nocrypt -topk8 -outform DER \
    -out selfupdate_test_key.der
```

# signed.exe and signed.exe.gz
`signed.exe` is an executable compiled from
[save_arguments.cc](https://chromium.googlesource.com/external/omaha/+/d3a3fcd2e3141534c2564c53309892f3f73afaa9/testing/save_arguments.cc),
and then authenticode signed with official Google LLC certificates with
thumbprints `a3958ae522f3c54b878b20d7b0f63711e08666b2` and
`cb7e84887f3c6015fe7edfb4f8f36df7dc10590e`.

`signed.exe.gz` is a gzipped version of `signed.exe`.

It is used in
[certificate tagging](https://source.chromium.org/chromium/chromium/src/+/main:docs/updater/design_doc.md;l=1113?q=tagging&ss=chromium%2Fchromium%2Fsrc:docs%2Fupdater%2F)
and networking unit tests.

# tagged_encode_utf8.exe
This is an executable compiled from
[save_arguments.cc](https://chromium.googlesource.com/external/omaha/+/d3a3fcd2e3141534c2564c53309892f3f73afaa9/testing/save_arguments.cc),
then authenticode signed with official Google LLC certificates with
thumbprints `a3958ae522f3c54b878b20d7b0f63711e08666b2` and
`cb7e84887f3c6015fe7edfb4f8f36df7dc10590e`, and finally tagged with the string
`"brand=QAQA"`.

It is used in
[certificate tagging](https://source.chromium.org/chromium/chromium/src/+/main:docs/updater/design_doc.md;l=1113?q=tagging&ss=chromium%2Fchromium%2Fsrc:docs%2Fupdater%2F)
unit tests.

# ProcmonConfiguration.pmc

This is a
[procmon](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon)
config file to limit events and reduce the PML/CSV size. It is used in unit and
integration tests.

The filters within the PMC file are as follows:
* Include events where the `Path` or `Detail` contain "updater".
* Include events where the `Path` or `Detail` contain "TypeLib" or
  "Interface".
* Include events where the `Process Name` contains: `updater` AND
    `Path` contains: `updater OR TypeLib OR Interface`
* Exclude procmon, procmon64, and most profiling events.

# app\_logos directory

Contains a sample application logo that is used in unit tests. More information
on app logos is
[here](https://source.chromium.org/chromium/chromium/src/+/main:docs/updater/functional_spec.md;l=1182?q=app.*logo).

# ChromiumMSI and GoogleMSI directories

These directories contain test MSI installers that are used in integration
tests. These installers are created using the script
[chrome/updater/test/test_installer/create_test_msi_installer.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/test/test_installer/create_test_msi_installer.py).
To regenerate these installers, the script can be run with parameters similar to
what is in the
[test_msi_installer template here](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/test/test_installer/BUILD.gn;l=102?q=%22%20%20template(%22test_msi_installer%22)%20%7B%22&ss=chromium).

# enterprise directory

The `enterprise` directory contains test ADMX/ADML files that are used in the
[build_group_policy_template_unittest.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/enterprise/win/google/build_group_policy_template_unittest.py)
unit test.

The test files were generated with the following apps as a parameter to the
[build_group_policy_template.py](https://source.chromium.org/chromium/chromium/src/+/main:chrome/updater/enterprise/win/google/build_group_policy_template.py)
script.

```
    TEST_APPS = [
        ('Google Test Foo', '{D6B08267-B440-4c85-9F79-E195E80D9937}',
         ' Check http://www.google.com/test_foo/.', 'Disclaimer', True, True),
        (u'Google User Test Foo\u00a9\u00ae\u2122',
         '{104844D6-7DDA-460b-89F0-FBF8AFDD0A67}',
         ' Check http://www.google.com/user_test_foo/.', '', False, True),
    ]
```

