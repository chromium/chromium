## Web App Shortcuts on macOS

Shortcuts on Mac are different from shortcuts on some other platforms, in
that -- behind the scenes -- they consist of, not a single file (as on some
other platforms), but a folder structure, which can be viewed and explored
with `ls` in a terminal. This folder structure is not visible in Finder
(unless revealed with 'Show Package Contents' in the Finder context menu),
and is normally represented in Finder as a single item (a shortcut, if you
will).

The shortcut structure, often also referred to as an 'app bundle', generally
resides in `~/Applications/[Chrome|Chrome Canary|Chromium] Apps.localized/`
but can be freely renamed and/or moved around the OS by the user.

When an app (let's call it 'Killer Marmot') is installed for the first time,
an app bundle is created with that same name inside the directory above. When
browsing to it with Finder, all the user sees is a single icon with the title
'Killer Marmot'. The folder structure, that is hidden behind it, looks
something like this:

```
Killer Marmot.app/          <- The app package (aka. the shortcut.)
 - Contents/
   - Info.plist              <- General information about the app bundle.
   - PkgInfo                 <- A file containing simply 'APPL????'.
   - MacOS/app_mode_loader   <- A generic binary that launches the app.
   - Resources/app.icons     <- The icons used by the shortcut.
   - Resources/en.proj/      <- One of potentially many resources directories
                                (swap out [en] for your locale).
     - InfoPlist.strings     <- The localized resources string file.
```

### Display name of an app

When determining which display name Finder should show for this app, it
considers information from up to three different sources:
- The filename of the App on disk, minus the `.app` suffix (which in this case
  would be: 'Killer Marmot').
- The `CFBundleName` value inside the `Contents/Info.plist` file.
- The `CFBundleDisplayName` value inside the `InfoPlist.strings` (from the right
  resource folder).
  - Note: Confusingly, a value for `CFBundleName`, is also written to the
  `InfoPList.strings` file, but it seems to not be used for anything and can
  therefore be ignored.

Of those three sources mentioned, Finder will start by considering the first
two (the filename on disk and `CFBundleName` from `Info.plist`) and if (and only
if) they match exactly will the display name (`CFBundleDisplayName` inside the
resource file) be used in Finder. If they don't match, however, Finder will
simply stop using the localized version (read: ignore the
`CFBundleDisplayName`) and instead use the filename of the app bundle folder on
disk (again, minus the .app suffix).

Note: When testing this locally, it is important to realise that manual
changes to the `CFBundleName` and/or `CFBundleDisplayName` values will be
ignored by Finder, even across reboots. Changes to those values therefore would
need to be done through the code that builds the shortcut. In contrast to that,
changes to the *filename* of the app bundle take effect immediately. So if,
after manually renaming, the new app folder name no longer matches
`CFBundleName`, Finder will immediately stop using `CFBundleDisplayName`.
Conversely, renaming the app folder back to its original name will cause
Finder to start using the localized name again (`CFBundleDisplayName`).

OS-specific gotchas related to using paths/urls as (or part of) App names:

When MacOS decides to show the localized name (read `CFBundleDisplayName`) in
Finder, it will collapse multiple consecutive forward-slashes (`/`) found
within that value into a single forward-slash. This means that if
`CFBundleDisplayName` contains `'https://foo.com'`, it will be shown in Finder
as `'https:/foo.com'` [sic]. Also, even though colon (`:`) is not valid input
when the user specifies app bundle filenames manually (i.e. during rename), it
can be programmatically added to the `CFBundleDisplayName` which _will_ then be
shown in Finder. That presents a problem, however, when the user tries to
rename the app bundle, because MacOS won't accept the new name unless the
user manually removes the colon from the name, which is less than ideal UX.
Therefore, the use of colons in the display name should be discouraged.

Also, Chrome converts forward-slashes in the app title to colons before using
it as the filename for the app bundle. But if MacOS decides the filename (and
not the localized value) should be used as the display name, it will
automatically convert any colons it finds in the filename into '/' before
displaying. This means that if a url for foo.com is used as an app title, it
will be written to disk as `'https:::foo.com'` but displayed as
`'https///foo.com'` [sic].
