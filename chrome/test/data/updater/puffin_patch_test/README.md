## How to regenerate linux, mac and windows test puff files:

If changes are made to puffin_app_v1/main.cc or puffin_app_v2/main.cc, the various puff files which represent a patch between the crx's produced by each of these sources. Thus, we'll need to regenerate these on each platform.

This README assumes you are already in your Chromium repo's src directory, that your gn args were generated in "out/Default", and that you are able to build the third_party/puffin:puffin target. Eventually, puffin will always be a valid target, but currently it requires you to link it into the build somehow. For now, you may need to temporarily add the following to your "//third_party/BUILD.gn"

    group("puffin") {
      testonly = true
      deps = [ "//third_party/puffin"]
    }

*Be careful not to submit this change in the final cl!*

**Linux commands**

    autoninja -C out/Default puffin puffin_app_v1_crx puffin_app_v2_crx
    rm chrome/test/data/updater/puffin_patch_test/linux_v1_to_v2.puff
    rm chrome/test/data/updater/puffin_patch_test/linux_v2_to_v1.puff
    out/Default/puffin -puffdiff out/Default/puffin_app_v1.crx3 out/Default/puffin_app_v2.crx3 chrome/test/data/updater/puffin_patch_test/linux_v1_to_v2.puff
    out/Default/puffin -puffdiff out/Default/puffin_app_v2.crx3 out/Default/puffin_app_v1.crx3 chrome/test/data/updater/puffin_patch_test/linux_v2_to_v1.puff

**Mac commands**

    autoninja -C out/Default puffin puffin_app_v1_crx puffin_app_v2_crx
    rm chrome/test/data/updater/puffin_patch_test/mac_v1_to_v2.puff
    rm chrome/test/data/updater/puffin_patch_test/mac_v2_to_v1.puff
    out/Default/puffin -puffdiff out/Default/puffin_app_v1.crx3 out/Default/puffin_app_v2.crx3 chrome/test/data/updater/puffin_patch_test/mac_v1_to_v2.puff
    out/Default/puffin -puffdiff out/Default/puffin_app_v2.crx3 out/Default/puffin_app_v1.crx3 chrome/test/data/updater/puffin_patch_test/mac_v2_to_v1.puff

**Windows commands**

    autoninja -C out\Default puffin puffin_app_v1_crx puffin_app_v2_crx
    del /f  chrome\test\data\updater\puffin_patch_test\win_v1_to_v2.puff
    del /f  chrome\test\data\updater\puffin_patch_test\win_v2_to_v1.puff
    out\Default\puffin.exe -puffdiff out\Default\puffin_app_v1.crx3 out\Default\puffin_app_v2.crx3 chrome\test\data\updater\puffin_patch_test\win_v1_to_v2.puff
    out\Default\puffin.exe -puffdiff out\Default\puffin_app_v2.crx3 out\Default\puffin_app_v1.crx3 chrome\test\data\updater\puffin_patch_test\win_v2_to_v1.puff

## Testing the new patches
You can test but running the following commands to verify if all tests pass, on each platform. Specifically the "PatchingTest.ApplyPuffPatchTest":

**Mac and Linux:**
    autoninja -C out/Default puffin_unittest
    out/Default/puffin_unittest

**Windows:**
    autoninja -C out\Default puffin_unittest
    out\Default\puffin_unittest.exe