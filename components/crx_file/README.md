# CRX File

The CRX File component is a collection of compilation units related to the
creation or manipulation of CRX archive files.

A CRX file is a ZIP archive with a prepended header section. The CRX file format
is described in more detail in `crx3.proto`.

`crx_creator.h` provides a means to create a CRX file.

`crx_verifier.h` provides a means to verify the integrity of a CRX file.
