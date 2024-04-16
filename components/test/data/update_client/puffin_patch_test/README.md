## How to regenerate linux, mac and windows test puff files:

If changes are made to puffin_app_v1/main.cc or puffin_app_v2/main.cc, the various puff files which represent a patch between the crx's produced by each of these sources. Thus, we'll need to regenerate these on each platform.

This README assumes you are already in your Chromium repo's src directory, that your gn args were generated in "src/out/Default", and that you are able to build the third_party/puffin:puffin target. To make sure puffin is building, for the time being we have to add the following to our gn args:

    enable_puffin_patches = true

<!-- TODO(crbug.com/40855693) once the enable_puffin_patches build argument is removed, we should update this documentation. -->

**Linux and Mac commands**

    autoninja -C out/Default puffin puffin_app_v1 puffin_app_v2
    rm components/test/data/update_client/puffin_patch_test/puffin_app_v1_to_v2.puff
    rm components/test/data/update_client/puffin_patch_test/puffin_app_v2_to_v1.puff
    out/Default/puffin -puffdiff out/Default/puffin_app_v1.crx3 out/Default/puffin_app_v2.crx3 components/test/data/update_client/puffin_patch_test/puffin_app_v1_to_v2.puff
    out/Default/puffin -puffdiff out/Default/puffin_app_v2.crx3 out/Default/puffin_app_v1.crx3 components/test/data/update_client/puffin_patch_test/puffin_app_v2_to_v1.puff
    cp out/Default/puffin_app_v1_crx.crx3 components/test/data/update_client/puffin_patch_test/puffin_app_v1.crx3
    cp out/Default/puffin_app_v2_crx.crx3 components/test/data/update_client/puffin_patch_test/puffin_app_v2.crx3

**Windows commands**

    autoninja -C out\Default puffin puffin_app_v1_crx puffin_app_v2_crx
    del /f  components\test\data\update_client\puffin_patch_test\puffin_app_v1_to_v2.puff
    del /f  components\test\data\update_client\puffin_patch_test\puffin_app_v2_to_v1.puff
    out\Default\puffin.exe -puffdiff out\Default\puffin_app_v1.crx3 out\Default\puffin_app_v2.crx3 chrome\test\data\updater\puffin_patch_test\puffin_app_v1_to_v2.puff
    out\Default\puffin.exe -puffdiff out\Default\puffin_app_v2.crx3 out\Default\puffin_app_v1.crx3 chrome\test\data\updater\puffin_patch_test\puffin_app_v2_to_v1.puff
    cp out\Default\puffin_app_v1_crx.crx3 components\test\data\update_client\puffin_patch_test\puffin_app_v1.crx3
    cp out\Default\puffin_app_v2_crx.crx3 components\test\data\update_client\puffin_patch_test\puffin_app_v2.crx3

## Testing the new patches
You can test but running the following commands to verify if all tests pass, on each platform. Specifically the "PatchingTest.ApplyPuffPatchTest":

**Mac and Linux:**
    autoninja -C out/Default puffin_unittest
    out/Default/puffin_unittest

**Windows:**
    autoninja -C out\Default puffin_unittest
    out\Default\puffin_unittest.exe
