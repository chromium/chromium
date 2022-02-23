# ChromeDriver Release Tools

This directory contains scripts and templates needed for ChromeDriver release process.
Script `nest` is an umbrella script routing user commands to the appropriate
`nest-${command}` script.
You don't need to execute the other scripts directly.

TIP: Add this directory to your `PATH` environmental variable and enjoy out
of source release process.
Alternatively you can symlink `nest` into a directory
that is already in your `PATH`.

NOTE: The scripts can run on Linux platform only!

Directory [templates](templates) contains the following templates
used for release announcements:

* [template1.html](templates/template1.html),
  [template2.html](templates/template2.html) -
  release announcement email templates for one or two versions of ChromeDriver respectively.
* [parametrized_change_list.html](templates/parametrized_change_list.html) -
  change list template parametrized with ChromeDriver version.
  It looks as follows in the generated email:

  ```
  Changes in version 95 of ChromeDriver include:

  ${LI_LIST}
  ```

  where `${LI_LIST}` is the actual release note list for the given version.
  We use this template while announcing a release of two versions simultaneously.
* [static_change_list.html](templates/static_change_list.html) -
  change list template that does not mention ChromeDriver version.
  It looks as follows in the generated email:

  ```
  Changes in this version of ChromeDriver include:

  ${LI_LIST}
  ```

  where `${LI_LIST}` is the actual release note list.
  We use this template for single version release announcement.
* [li.html](templates/li.html) - template for release note entry in
  release announcement email.
  The actual release note replaces `${LI_TEXT}` macro in the template.
* [template1.txt](templates/template1.txt),
  [template2.txt](templates/template2.txt) -
  templates for *text/plain* part of release announcement email.
* [blog_home_template1.md](templates/blog_home_template1.md),
  [blog_home_template2.md](templates/blog_home_template2.md) -
  templates for release announcement at
  [Home](https://chromedriver.chromium.org/)
  page of ChromeDriver site.
* [blog_download_template1.md](templates/blog_download_template1.md),
  [blog_download_template2.md](templates/blog_download_template2.md) -
  templates for release announcement at
  [Downloads](https://chromedriver.chromium.org/downloads)
  page of ChromeDriver site.


## Releasing a new version of ChromeDriver

NOTE: We assume that path to `nest` is in your `PATH` variable.

You need to execute the following steps:

1. Go to the following URL to review bug fixed in the release, replacing the
  trailing 81 with actual ChromeDriver major version number:

  https://crbug.com/chromedriver?can=1&sort=id&q=label:ChromeDriver-81

1. After reviewing the above page and making all the necessary updates, click on the
  `CSV` link near the lower-right corner to download the bug list to
  `~/Download/chromedriver-issues.csv`.

1. Switch to any empty directory.

1. Generate release notes executing

  ```bash
  nest note ${version}`
  ```

  where `${version}` is the full
  [4-part version number](https://www.chromium.org/developers/version-numbers)
  of the new release.
  The script reads `~/Download/chromedriver-issues.csv`,
  and saves the resulting file in `notes\_${version}.txt` of the current directory.

1. Copy ChromeDriver binaries and release notes to the release web site.

   ```bash
  nest cut-out ${release_type} ${version}
  ```

  The first argument `${release_type}` is the release type: *stable* or *beta*.
  The second argument `${version}` is the full 4-part version number of the new release.
  Make sure that this command finishes with message **Success!!!**.

1. In case if you want to release and announce two versions in one shot you need to repeat
  the steps above again.

1. Generate the release announcements.

  ```bash
  nest announce
  ```

  They will be written by the script to the following files:

  * `./blog_home.md` - contains messages for pasting to
    [Home](https://chromedriver.chromium.org/) page of ChromeDriver site.
  * `./blog_downloads.md` - contains messages for pasting to
    [Downloads](https://chromedriver.chromium.org/downloads) page of ChromeDriver site.
  * `./announcement.eml` - announcement email.
    You can send it via Thunderbird or copy/paste it to your email client.

  This script is able to recognize whether you want to announce one or two
  versions and it will generate the aforementioned files correctly combining
  all the needed information.

1. Check
  [chromedriver storage site](https://chromedriver.storage.googleapis.com/index.html)
  to verify binaries have been released.


## Troubleshooting

* If you encounter error that `gsutil` can't be found, install it with:

  ```bash
  sudo apt-get install google-cloud-sdk
  ```

* If you encounter permission errors from gsutil, run the following command and
  login with an account that has permission to update gs://chromedriver.

  ```bash
  gcloud auth login
  ```

* If `LATEST_RELEASE` file needs updating, it can be done manually, e.g.,

  ```bash
  gsutil cp gs://chromedriver/LATEST_RELEASE_81 gs://chromedriver/LATEST_RELEASE
  ```


## Development

For convenience of development and troubleshooting `nest cut-out` can be
executed by parts:

1. Create a working directory for the release process.

  ```bash
  nest create ${release_type} ${version}
  ```

  The first argument `${release_type}` is the release type: *stable* or *beta*.
  The second argument `${version}` is the full 4-part version number of the new release.
  You can also use two shortcuts for this command:

  ```bash
  nest stable ${version}
  nest beta ${version}
  ```

1. Descend to the directory created above.

  ```bash
  cd ${version}
  ```

1. Download the binaries that need to be published.

  ```bash
  nest pull
  ```

  The archives are placed into `crome-unsigned` folder.
  In the same step they are extracted to `unzipped` folder.

1. Create the archives that need to be published,
  gather all the information needed for release in folder `packed`.

  ```bash
  nest pack [--notes=path-to-release-notes]
  ```

  If you omit `--notes` argument the script tries to find the rlease notes under
  the release working directory, named as `${version}`, and one level above it.
  This step also verifies that the release notes indeed refer the version being
  prepared.

1. Publish the release prepared in folder `packed`.

  ```bash
  nest push
  ```

  If the release is *stable* then this script also updates
  gs://chromedriver/LATEST_RELEASE accordingly.

1. Pull the released binaries and notes and compare their hashes
  with the binaries in `unzipped` folder and the notes in `packed` folders.

  ```bash
  nest verify
  ```

## How to update the templates

This task is performed manually as follows:

1. Update the announcement letter text in
  [ChromeDriver Release Process](http://go/chromedriver-release-process).
1. Copy it and paste it with the command

  ```bash
  xclip -selection c -o -t text/html > templates/template2.html
  ```

1. Extract the \<li\>..\</li\> part into [templates/li.html](templates/li.html).
  Replace it with `${LI_LIST}` in [templates/template2.html](templates/template2.html)

1. Extract the block containing text *"Changes in version ${MAJOR}..."* and the
   following \<ul\>..\</ul\> into
   [templates/parametrized_change_list.html](templates/parametrized_change_list.html)
   and replace them with macro `${CHANGE_LIST}` in [templates/template2.html](templates/template2.html).

1. Copy [templates/parametrized_change_list.html](templates/parametrized_change_list.html)
  to [templates/static_change_list.html](templates/static_change_list.html)
  and change the text in the destination accordingly.

1. Copy [templates/template2.html](templates/template2.html)
  to [templates/template1.html](templates/template1.html)
  and change the text in the destination accordingly:
  * Remove any mentions of `${VERSION2}`, `${MAJOR2}`, `${TYPE2}`, `${KIND2}`.
  * Give attention to replacing plural forms of words with singular forms.

1. Copy [templates/template2.html](templates/template2.html)
  as plain text into [templates/template2.txt](templates/template2.txt)

1. Copy [templates/template1.html](templates/template1.html)
  as plain text into [templates/template1.txt](templates/template1.txt)

The templates in
[templates/blog_home_template1.md](templates/blog_home_template1.md),
[templates/blog_home_template2.md](templates/blog_home_template2.md),
[templates/blog_download_template1.md](templates/blog_download_template1.md),
and  [templates/blog_download_template2.md](templates/blog_download_template2.md)
are not related to [ChromeDriver Release Process](http://go/chromedriver-release-process).
You can update them directly without any copy/paste.

