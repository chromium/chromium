# Behavior of Real Time URL Allowlist Checks in Chrome on Android

This describes how a dev can create a new allowlist component version with an updated
version of the allowlist from Safe Browsing and push it to GCS. Once the newly generated
component is in GCS, the component updater will provide Chrome with this new version of the
allowlist.

## Procedure for updating the allowlist by creating a new component
* **Edit** `real_time_url_allowlist.asciipb` file.
  * The version_id should be the current pb's version_id incremented by 1.
  * The scheme_id should not be changed, unless there are backwards-incompatible changes made
  such that the previous version of the code is no longer compatible with the latest version
  of the component. This will block clients that are using older versions of the code from
  upgrading to the latest component version until they update to the latest version of the
  code. This should be used carefully, since while it ensures backwards compatibility, it
  also makes it so that we can no longer update the component for clients with older code.
  Therefore, the new scheme_id should be incremented by 1 only if such changes are made to
  the code.
  * Do not change the url_hashes, since they will be fetched and stored in the next step.
* **Run** `python3 store_real_time_url_allowlist_prefixes.py -p ~/chromium/src/out/{Android}`,
  where {Android} is the name of your Android build directory. Run this from the
  `components/safe_browsing/content/resources/real_time_url_checks_allowlist/` directory.
  This script will use the Safe Browsing API to fetch the URL hash prefixes, validate
  them, and if they're valid, store them in the real_time_url_allowlist.asciipb file.
    * To double check that the new allowlist contents are valid, **run**
    `~/chromium/src/out/{Android}/components_unittests --gtest_filter="RealTimeUrlChecksAllowlistResourceFileTest.*"`
    before proceeding
* **Submit** a chromium CL with the new `real_time_url_allowlist.asciipb` version.
    * When chrome builds, it will automatically run the make_real_time_url_allowlist_protobuf
      target. This target builds the new version of the local real_time_url_allowlist.pb file.
    * Wait 1 week for this to run on Canary. Then, verify there have been no crashes and that
      there are no regressions on UMA for the metrics listed below for Android. (If either of
      these issues happen, follow the rollback procedure described in the next section.)
        * Allowlist match rate
          (SafeBrowsing.RT.LocalMatch.Result)
        * Number of times SB is the navigation bottleneck
          (SafeBrowsing.BrowserThrottle.IsCheckCompletedOnProcessResponse)
        * Time to contentful paint
          (PageLoad.PaintTiming.NavigationToFirstContentfulPaint)
* **Push** the new version of the component to GCS to roll it out to users.
    * In a synced checkout, run the following to generate the proto and push it to GCS. Run this
      from the `/chromium/src/` directory. Replace the arg with your build directory:
        * % `components/safe_browsing/content/resources/real_time_url_checks_allowlist/push_real_time_url_allowlist_proto.py -d out-gn/Android`
    * It will ask you to double check its actions before proceeding.  It will fail if you're not
      a member of `chrome-counter-abuse-core@google.com`, since that's required for access to
      the GCS bucket.
    * The Component Updater system will notice those files and gradually push them to users
      over a 3 day period. If not, contact the Omaha support team.

## Procedure for rollback
While Omaha allows rollback through the release manager, the Chrome client will
reject updates with lower version numbers. (This is important for running new
versions on Canary/Dev channel). Rolling back a bad version is best achieved by:
  * **Reverting** the changes on the Chromium source tree.
  * **Submitting** a new CL incrementing the version number.
  * **Push** the newest version, as above.