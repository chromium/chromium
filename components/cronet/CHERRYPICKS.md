# Cherrypicks guide

Files in //components/cronet are never a part of Chromium build. Instead, they are exclusively a part of [Cronet](README.md). Therefore, there is little value in this code going through the Chromium's verification process. Instead, we will eagerly cherrypick changes as needed, based on the stability signals from devices where the code is actually released.

Code submitted to *main* branch is released weekly for testing with the internal Android builds. Code in the *Stable* track is released to the production Android devices. This guide aims to guarantee the changes that go to production devices are first tested for at least two weeks in internal builds.

## Cherrypick to Beta

A change is considered safe to cherrypick to *Beta* when:

  * The **next Stable Cut** is more than 2 weeks away (which allows for testing in Android weekly internal builds). Check [the dashboard for promotion dates](https://chromiumdash.appspot.com/schedule) and if in doubt, simply don't cherrypick to *Beta* changes that were originally merged to *main* less than 2 weeks ago.

The usual [Chromium cherrypick process](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/process/merge_request.md) can be followed to trigger such change. Cherrypicks to beta are tested and autoapproved by a bot - but we are enforcing the above ruleset as a best effort during the review process.

## Cherrypick to Stable

A change is considered safe to cherrypick to *Stable* when:

  *  **All** of the files in the change are scoped only to //components/cronet
  *  It has been originally merged to *main* for at least 2 weeks (which allows for testing in Android weekly internal builds)