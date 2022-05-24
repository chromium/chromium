# Chrome Cleanup Tool

This directory contains the source code for the Chrome Cleanup Tool, a
standalone application distributed to Chrome users to find and remove Unwanted
Software (UwS).

The tool and its tests are Windows-only.

## Integration with Chrome

The application is distributed in two versions:

1.  A Chrome component named the Software Reporter that finds UwS but does not
    have the ability to delete anything.
2.  A separate download named Chrome Cleanup Tool that both finds and removes UwS.

The Software Reporter runs in the background and, if it finds UwS that can be
removed, reports this to Chrome. Chrome then downloads the full Cleanup Tool
and shows a prompt on the settings page asking the user for permission to
remove the UwS.

This directory contains the source for both.

Code in Chromium that deals with the Software Reporter Tool and Chrome Cleanup
Tool includes:

*   [Software Reporter component updater](/chrome/browser/component_updater)
    (files `sw_reporter_installer_win*`)
*   [Chrome Cleanup Tool fetcher and launcher](/chrome/browser/safe_browsing/chrome_cleaner)
*   [Settings page resources](/chrome/browser/resources/settings/chrome_cleanup_page)
*   [Settings page user interaction handlers](/chrome/browser/ui/webui/settings)
    (files `chrome_cleanup_handler.*`)
*   [UI for modal dialogs](/chrome/browser/ui/views) (files `chrome_cleaner_*`)
*   [Shared constants and IPC interfaces](/components/chrome_cleaner/public) -
    These are used for communication between Chrome and the Software Reporter /
    Chrome Cleanup Tool, so both this directory and the other Chromium
    directories above have dependencies on them.

## Targets

To build all targets for this project, use:

//chrome/chrome_cleaner:chrome_cleaner

The main build targets are:

 *   //chrome/chrome_cleaner:software_reporter_tool
 *   //chrome/chrome_cleaner:chrome_cleanup_tool
 *   //chrome/chrome_cleaner:chrome_cleaner_unittests

There is also a tool, `generate_test_uws`, which will create some harmless text
files that the tool will detect as UwS:

*   //chrome/chrome_cleaner/tools:generate_test_uws

## Internal resources

The public build will link to the test scanning engine in
`chrome/chrome_cleaner/engines/target/test_engine_delegate.cc` which only
detects test files. This is the default when building on the Chromium
buildbots.

If `is_internal_chrome_cleaner_build` is set in GN, the build looks for
internal resources in the `chrome_cleaner/internal` directory. These resources
are not open source so are only available internally to Google. They include
the licensed scanning engine used to find real-world UwS.

To ship a non-test version of the tool, implement EngineDelegate to wrap an
engine that can detect and remove UwS. The engine will be run inside a sandbox
with low privileges. To perform operations like opening file handles, scanning
process memory, and deleting files, the engine will need to call the callbacks
passed to EngineDelegate as `privileged_file_calls`, `privileged_scan_calls`
and `privileged_removal_calls`.

### Getting the internal resources through gclient

To check out the internal resources set both of these to True in .gclient:

*  `checkout_src_internal` (standard argument defined in DEPS)
*  `checkout_chrome_cleaner_internal` (defined in src-internal's DEPS, causes
   the internal resources to be checked out under
   `chrome/chrome_cleaner/internal`)

To actually build with the internal resources, also set
`is_internal_chrome_cleaner_build` to true in args.gn.

## Build arguments

The build is controlled by the following arguments that can be set in args.gn:

*  `is_internal_chrome_cleaner_build`: If true, GN targets will depend on
   targets in chrome/chrome_cleaner/internal, otherwise will depend only on
   public resources.
*  `is_official_chrome_cleaner_build`: If true, various development options
   will be disabled since the build is meant for release to end users.
*  `enable_software_reporter`: This is true by default for `is_chrome_branded`
   builds, but can be set to false if a Chrome branded build is needed but
   Software Reporter execution is not required.
*  `reporter_branding_path`, `cleaner_branding_path`, `version_path`: Paths to
   resource files that will be used to populate the VERSIONINFO of the
   executables.
   * By default these identify as The Chromium Authors. When
   `is_internal_chrome_cleaner_build` is set, these are overridden to identify
   as Google.
   * To ship a customized version of the tool, override these to point
   to files identifying the authors of the custom version.

## Contact

Please file bugs / feature requests / inquiries using the
[Services>SafeBrowsing>ChromeCleanup component](https://bugs.chromium.org/p/chromium/issues/entry?components=Services%3ESafebrowsing%3EChromeCleanup)
for tracking.