# How to generate installer test files

This README describes how to generate:
- test_chrome.7z
- test_chrome.packed.7z
- mini_installer.exe.test
- test_mini_installer_uncompressed.exe.test

These files are needed for mini_installer_unittest and unpack_archive_unittest

## Build the real mini_installers

On a windows machine, generate a new build directory, for this example:

    gn args out\Default

When prompted, set the build arguments to the following:

    is_component_build = false
    target_cpu = "x86"
    is_debug = false
    # Required to build mini_installer_uncompressed:
    enable_uncompressed_archive=true

Next, build the `mini_installer` and the `mini_installer_uncompressed` targets,
to produce a release build of `mini_installer.exe` and
`mini_installer_uncompressed.exe`:

    autoninja -C out/Default mini_installer mini_installer_uncompressed


This will also build the *mini_installer_archive* target, which produces the
uncompressed `chrome.7z` and the compressed `chrome.packed.7z` contained in the
mini_installer binaries.

## Create the fake chrome archives

In order to reduce the size of the test files stored in the repository, replace
the files contained in `chrome.7z` with a file to verify the archives are
unpacked as expected, regardless of contents.

This can be done by opening a blank text file, and pasting the following string
as is without any new lines, null terminator, or preceding or trailing
whitespace:

    fakechromiumdata

Save this file as `test_data.txt` to a path you will use later.

Run the following commands to modify the archives using the 7zip commandline
utility 7za.exe provided in the repository:

```
:: Delete all the files within chrome.7z
"{chromium source dir}\third_party\lzma_sdk\bin\host_platform\7za.exe" d ^
"{chromium source dir}\out\Default\chrome.7z" * -r
:: Delete all the files within chrome.packed.7z
"{chromium source dir}\third_party\lzma_sdk\bin\host_platform\7za.exe" d ^
"{chromium source dir}\out\Default\chrome.packed.7z" * -r
:: Add the test_data.txt to the chrome.7z
"{chromium source dir}\third_party\lzma_sdk\bin\host_platform\7za.exe" a ^
"{chromium source dir}\out\Default\chrome.7z" "{path}\{to}\test_data.txt"
:: Add the chrome.7z to the chrome.packed.7z
"{chromium source dir}\third_party\lzma_sdk\bin\host_platform\7za.exe" a ^
"{chromium source dir}\out\Default\chrome.packed.7z" ^
"{chromium source dir}\out\Default\chrome.7z"
```

Retain the modified `chrome.7z` and `chrome.packed.7z` for later.

## Create the fake setup binaries

Next, create a fake `SETUP.EXE`. This can be done by opening a blank text file,
and pasting the following string as is without any new lines, null terminator,
or preceding or trailing whitespace:

    fakesetupdata

Save this file as `SETUP.EXE`.

Take the fake `SETUP.EXE` and use it to produce the fake compressed `SETUP.EX_`
cab using windows `makecab` tool:

    makecab "{input}\{path}\SETUP.EXE" "{output}\{path}\SETUP.EX_"

Save the fake, compressed `SETUP.EX_` for later.

## Create the fake component resource

Next, open a blank text file and paste the following string as is without any
new lines, null terminator, or preceding or trailing whitespace:

    fakebddata

Save the file as `bdresources.txt` for later.

## Modify the mini_installer executable resources

Next, open Visual Studio, and create a new project/solution.

*NOTE: This project won't actually be needed after modifying the resources,
but it's important to note that if `mini_installer.exe` is opened as a
`project/solution`, it does not give provide the opportunity to modify the
resources.*

Select `File > Open > File...` and select the `out\Default\mini_installer.exe`
that was built above, which will present visual studio's resource viewer.
This allows for a developer edit the existing resources.

To finish producing the fake mini_installers, a developer will need to replace
and add a few resources in order to prepare these fake mini_installers for
testing.

For example, for the `mini_installer.exe`, the developer will need to replace
the resource located at `mini_installer.exe/"B7"/CHROME.PACKED.7Z` with the fake
`CHROME.PACKED.7Z` produced above.

This can be done by right-clicking anywhere on the `mini_installer.exe` resource
directory view, and then selecting `Add Resource` from the drop-down menu.

In the menu that appears, click the `Import...` button which will open a file
explorer. In the file explorer menu, make sure that `All Files (*.*)` filter is
selected, as the default `Bitmaps` will filter out the relevant files.

Navigate to the file that will be imported--in this case, the fake
`chrome.packed.7z` archive--select it and click the `Open` button.

In the `Custom Resource Type` type in the desired resource type with quotes
(`"B7"` for LZMA compressed resources, `"BL"` for LZC compressed resources,
`"BN"` for uncompressed resources, or `"BD"` for component resources), in
this case `"B7"`, and click `OK`.

This opens the file in visual studio as an `*.RCDATA` file. hit `Ctrl + S` or
manually save this file to create the resource and return to the
`mini_installer.exe` tab to see the resource has been created, but with a
resource name set to some integer, let's assume `101`.

If a resource is being added, which should be the case for `"BD"` resources for
component builds, simply continue on to the next step. If a resource is being
replaced, take a note of the exact name of the original resource ID for the
resource to be replaced, right-click and select `Delete` for the resource being
replaced.

Click on the new resource, in this case `101`, and look for the ID field in the
`Custom Editor` panel on the right side of visual studio. If this is a resource
addition, replace this numerical ID with the original filename for the file you
imported *surrounded by quotes*. If this is a resource replacement, replace the
assigned numerical ID with the original name of the resource that was noted
above, *surrounded by quotes*. In this case, that would be `"CHROME.PACKED.7Z"`.

*NOTE: If the encapsulating quotes are not provided, visual studio will report
that this resource ID contains illegal characters and will refused to modify
it.*

For each executable, replace and add the resources as outlined below:

### mini_installer.exe resources

    Replace `mini_installer.exe\"B7"\"CHROME.PACKED.7Z"`
    with the modified `chrome.packed.7z` produced above.

    Replace `mini_installer.exe\"BL"\"SETUP.EX_"`
    with the newly compressed cab `SETUP.EX_`.

    Add `mini_installer.exe\"BD"\"bdresource.txt"`


### mini_installer_uncompressed.exe resources

    Replace `mini_installer_uncompressed.exe\"BN"\"CHROME.7Z"`
    with the modified `chrome.7z` produced above.

    Replace `mini_installer_uncompressed.exe\"BN"\"SETUP.EXE"`
    with the uncompressed `SETUP.EXE` from above.

    Add `mini_installer.exe\"BD"\"bdresource.txt"`




## Move and Rename the files
Now that all of the files are created, the last step is to move them to this
folder, `chrome\test\data\installer`, renaming the files as described below.

### File Produced -> Filename Expected by Tests

    `mini_installer.exe`                -> `mini_installer.exe.test`

    `mini_installer_uncompressed.exe`   -> `mini_installer_uncompressed.exe.test`

    `chrome.7z`                         -> `test_chrome.7z`

    `chrome.packed.7z`                  -> `test_chrome.packed.7z`

All other files created along the way to produce the above 4 packages can be
deleted as they are no longer needed.
