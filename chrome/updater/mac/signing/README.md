# Testing
1. Create a self-signed certificate, e.g. with identity `myid`, or use the
ad-hoc signing identity `'-'`.
2. `autoninja -C out/MyOutDir chrome/updater`
3. ```
     out/MyOutDir/Updater\ Packaging/sign_updater.py --identity myid \
     --development --input out/MyOutDir --output MySignedOutputDir \
     --notarize=none
   ```
