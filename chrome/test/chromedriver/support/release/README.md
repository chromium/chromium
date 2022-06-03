This directory contains scripts used by ChromeDriver team to make public
releases of ChromeDriver.

The following steps are involved:

* Go to the following URL to review bugs fixed in the release, replacing the
  trailing 81 with actual ChromeDriver major version number:

  https://crbug.com/chromedriver?can=1&sort=id&q=label:ChromeDriver-81

* After reviewing the above page and making and necessary updates, click on the
  "CSV" link near the lower-right corner to download the bug list to
  ~/Download/chromedriver-issues.csv.

* Run Python script `release_notes.py` to generate release notes. It requires
  one command line argument, which is the full
  [4-part version number](https://www.chromium.org/developers/version-numbers)
  of the new release.
  The script is hardcoded to read ~/Download/chromedriver-issues.csv,
  and save the resulting file in notes.txt in the current directory.

* Run shell script `release.sh` to copy ChromeDriver binaries and release notes
  to the release web site. This script takes one argument, the full 4-part
  version number of the new release.

* Check
  [chromedriver storage site](https://chromedriver.storage.googleapis.com/index.html)
  to verify binaries have been released

Notes:

* If you encounter error that gsutil can't be found, install it with:

    sudo apt-get install google-cloud-sdk

* If you encounter permission errors from gsutil, run the following command and
  login with an account that has permission to update gs://chromedriver.

    gcloud auth login

* If `LATEST_RELEASE` file needs updating, it can be done manually, e.g.,

    gsutil cp gs://chromedriver/LATEST_RELEASE_81 gs://chromedriver/LATEST_RELEASE
