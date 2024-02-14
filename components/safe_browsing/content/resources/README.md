# Behavior of Download File Types in Chrome

This describes how to adjust file-type download behavior in
Chrome including interactions with Safe Browsing. The metadata described
here, and stored in `download_file_types.asciipb`, will be both baked into
Chrome released and pushable to Chrome between releases (via
`FileTypePolicies` class).  http://crbug.com/596555

Rendered version of this file: https://chromium.googlesource.com/chromium/src/+/main/components/safe_browsing/content/resources/README.md


## Procedure for adding/modifying file type(s)
  * **Edit** `download_file_types.asciipb` and `enums.xml`.
  * Get it reviewed, **submit.**
  * **Push** it to all users via component update:
    * Wait 1-3 day for this to run on Canary to verify it doesn't crash Chrome.
    * In a synced checkout, run the following to generate protos for all
      platforms and push them to GCS. Replace the arg with your build directory:
        * % `components/safe_browsing/content/resources/push_file_type_proto.py -d
          out-gn/Debug`
    * It will ask you to double check its actions before proceeding.  It will
      fail if you're not a member of
      `chrome-file-type-policies-pushers@google.com`, since that's required for
      access to the GCS bucket.
    * The Component Updater system will notice those files and push them to
      users within ~6 hours. If not, contact `waffles@.`

## Procedure for rollback
While Omaha allows rollback through the release manager, the Chrome client will
reject updates with lower version numbers. (This is important for running new
versions on Canary/Dev channel). Rolling back a bad version is best achieved by:
  * **Reverting** the changes on the Chromium source tree.
  * **Submitting** a new CL incrementing the version number.
  * **Push** the newest version, as above.

## Procedure for incremental rollout
  * Open the Omaha Release manager at:
    https://omaharelease.corp.google.com/product/1436/cohorts
  * **Disable** the automatic push by changing the _Push Scheduler_ from
    `LATEST_TO_AUTO` to `NONE`. Then commit the changes by clicking _Commit
    Automation Changes_.
  * **Upload** the new version of the file types.
    * In a synced checkout, run the following to generate protos for all
      platforms and push them to GCS. Replace the arg with your build directory:
        * % `components/safe_browsing/content/resources/push_file_type_proto.py -d
          out-gn/Debug`
  * Create a new cohort.
    * Under _Cohorts_, click _Manage_, then _Create Subcohort_ of Auto.
    * Select a name and percentage for the new cohort.
    * Return to the product page.
  * **Push** to the new cohort.
    * Select _Edit Schedule_ for the new cohort, and select the newest file
      group and a time to roll out.
  * After the incremental rollout is complete, remember to set the _Push
    Scheduler_ back to `LATEST_TO_AUTO`

Note: Everything after disabling automation could be scripted through the
[ReleaseServiceManager](http://google3/java/com/google/installer/releasemanager/proto/releasemanager_stubby.proto).
An example script using this API is
[here](http://google3/googleclient/installer/tools/release/keystone/create_release_testing_cohorts.py).
If we regularly need incremental rollouts, it may be worth creating our own
scripts to do so reliably.

## Guidelines for a DownloadFileType entry:
See `download_file_types.proto` for all fields.

  * `extension`: (required) Value must be unique within the config. It should be
    lowercase ASCII and not contain a dot. If there _is_ a duplicate,
    first one wins. Only the `default_file_type` should leave this unset.

  * `uma_value`: (required) must be unique and match one in the
    `SBClientDownloadExtensions` enum in `enums.xml`.

  * `is_archive`: `True` if this filetype is a container for other files.
     Leave it unset for `false`.

  * `ping_setting`:  (required). This controls what sort of ping is sent
     to Safe Browsing and if a verdict is checked before the user can
     access the file.

    * `SAMPLED_PING`: Don't send a full Safe Browsing ping, but
       send a no-PII "light-ping" for a random sample of SBER users.
       This should be used for known safe types. The verdict won't be used.

    * `NO_PING`:  Don’t send any pings. This file is allowlisted. All
      NOT_DANGEROUS files should normally use this.
    * `FULL_PING`: Send full pings and use the verdict. All dangerous
      file should use this.

  * `platform_settings`: (repeated) Zero or more settings to differentiate
     behavior by platform. Keep them sorted by platform. At build time,
     this list will be filtered to contain exactly one setting by choosing
     as follows before writing out the binary proto.

       1. If there's an entry matching the built platform,
         that will be preferred. Otherwise,

       2. If there's a "PLATFORM_TYPE_ANY" (i.e. `platform` is not set),
       that will be used. Otherwise,

       3. The `default_file_type`'s settings will be filled in.

    **Warning**: When specifying a new `platform_settings` for a file type, be
    sure to specify values for all necessary settings. The `platform_settings`
    will override all of the `default_file_type`'s settings, and this may result
    in a change in behavior for `platform_settings` left unspecified. For
    example, see [crbug.com/946558](https://crbug.com/946558#c5).

  * `platform_settings.danger_level`: (required) Controls how files should be
    handled by the UI in the absence of a better signal from the Safe Browsing
    ping. This applies to all file types where `ping_setting` is either
    `SAMPLED_PING` or `NO_PING`, and downloads where the Safe Browsing ping
    either fails, is disabled, or returns an `UNKNOWN` verdict. Exceptions are
    noted below.

    The warning controlled here is a generic "This file may harm your computer."
    If the Safe Browsing verdict is `UNCOMMON`, `POTENTIALLY_UNWANTED`,
    `DANGEROUS_HOST`, or `DANGEROUS`, Chrome will show that more severe warning
    regardless of this setting.

    This policy also affects also how subresources are handled for *"Save As
    ..."* downloads of complete web pages. If any subresource ends up with a
    file type that is considered `DANGEROUS` or `ALLOW_ON_USER_GESTURE`, then
    the filename will be changed to end in `.download`. This is done to prevent
    the file from being opened accidentally.

    * `NOT_DANGEROUS`: Safe to download and open, even if the download
       was accidental. No additional warnings are necessary.
    * `DANGEROUS`: Always warn the user that this file may harm their
      computer. We let them continue or discard the file. If Safe
      Browsing returns a `SAFE` verdict, we still warn the user.
        * Note that file types at this level can affect how our partner team
          calculates the warning volume. Please reach out before adding a new
          file type under this danger level.
    * `ALLOW_ON_USER_GESTURE`: Potentially dangerous, but is likely harmless if
      the user is familiar with host and if the download was intentional. Chrome
      doesn't warn the user if both of the following conditions are true:

        * There is a user gesture associated with the network request that
          initiated the download.
        * There is a recorded visit to the referring origin that's older than
          the most recent midnight. This is taken to imply that the user has a
          history of visiting the site.

      In addition, Chrome skips the warning if the users preference enables Safe
      Browsing or the download was explicit (i.e.  the user selected "Save link
      as ..." from the context menu), or if the navigation that resulted in the
      download was initiated using the Omnibox.

    If the `SafeBrowsingForTrustedSourcesEnabled` policy is set and the download
    originates from a Trusted source, no warnings will be shown even for types
    with a `danger_level` of `DANGEROUS` or `ALLOW_ON_USER_GESTURE`.

  * `platform_settings.auto_open_hint`: (required).
    * `ALLOW_AUTO_OPEN`: File type can be opened automatically if the user
      selected that option from the download tray on a previous download
      of this type.
    * `DISALLOW_AUTO_OPEN`:  Never let the file automatically open.
      Files that should be disallowed from auto-opening include those that
      execute arbitrary or harmful code with user privileges, or change
      configuration of the system to cause harmful behavior immediately
      or at some time in the future. We *do* allow auto-open for files
      that upon opening sufficiently warn the user about the fact that it
      was downloaded from the internet and can do damage. **Note**:
      Some file types (e.g.: .local and .manifest) aren't dangerous
      to open. However, their presence on the file system may cause
      potentially dangerous changes in behavior for other programs. We
      allow automatically opening these file types, but always warn when
      they are downloaded.
  * `platform_settings.max_file_size_to_analyze`: (optional).
      Size in bytes of the largest file that the analyzer is willing to inspect;
      for instance, a zip file larger than the threshold will not be unpacked
      to allow scanning of the files within.

  * TODO(nparker): Support this: `platform_settings.unpacker`:
     optional. Specifies which archive unpacker internal to Chrome
     should be used. If potentially dangerous file types are found,
     Chrome will send a full-ping for the entire file. Otherwise, it'll
     follow the ping settings. Can be one of UNPACKER_ZIP or UNPACKER_DMG.

## Guidelines for the top level DownloadFileTypeConfig entry:
  * `version_id`: Must be increased (+1) every time the file is checked in.
     Will be logged to UMA.

  * `sampled_ping_probability`: For what fraction of extended-reporting
    users' downloads with unknown extensions (or
    ping_setting=SAMPLED_PING) should we send light-pings? [0.0 .. 1.0]

  * `file_types`: The big list of all known file types. Keep them
     sorted by extension.

  * `default_file_type`: Settings used if a downloaded file is not in
    the above list. `extension` is ignored, but other settings are used.
    The ping_setting should be FULL_PING for all platforms, so that
    unknown file types generate pings.
