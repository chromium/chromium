# Views Examples Mac Signing

This directory contains the packaging and signing configuration for the
[Views Examples](../../../../ui/views/examples/) app. It reuses the Chrome Mac
signing pipeline from
[`//chrome/installer/mac/signing/`](../../../installer/mac/signing/), overriding
only the product specific parts (`config_factory.py`, `parts.py`, `pipeline.py`
and the config).

It lives under `//chrome` rather than next to Views Examples because `//ui`
must not depend on `//chrome`.

# Invoking

Signing requires a statically linked build (i.e. `is_component_build = false`),
which you can set up in a new GN out directory with the following args:

    is_debug = false
    is_component_build = false

# Testing
1. Obtain a signing identity (`myid` below). Googlers can use the
   [internal development identity](https://goto.google.com/ioscerts); otherwise
   you must supply your own. Note that a
   [self-signed](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/Procedures/Procedures.html)
   or ad-hoc (`-`) identity is incompatible with the _library validation_
   signing option that Views Examples, like Chrome, uses: the app will sign and
   verify, but will not launch.
2. `autoninja -C out/MyOutDir views_examples`
   (on Mac this builds both `Views Examples.app` and the
   `Views Examples Packaging` directory via the `//:views_examples` group).
3. ```
     out/MyOutDir/Views\ Examples\ Packaging/sign_views_examples.py --identity myid \
     --development --input out/MyOutDir --output MySignedOutputDir \
     --notarize=none
   ```

The Python unit tests can be run with `./run_mac_signing_tests.py`.
