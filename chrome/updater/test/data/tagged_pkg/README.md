# How to create a tagged PKG file for testing

To create a tagged PKG file for testing, you need a valid PKG file, the
`tag_exe` tool, and a Mac with Xcode command line tools installed.

1.  **Create a dummy PKG file**:
    You can use `pkgbuild` to create a dummy package.
    ```bash
    pkgbuild --identifier com.google.Chrome.Test --version 1.0.0.0 \
             --nopayload test.pkg
    ```

2.  **Inject a tag**:
    Use the `tag_exe` tool to inject a tag into the PKG file.
    ```bash
    out/Default/tag_exe --tag="brand=GGLZ" --out=tagged.pkg test.pkg
    ```

The `tag_exe` tool will insert the tag before the notarization trailer if
present, or append it to the end of the file.
