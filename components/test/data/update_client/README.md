# Test files

## gndmhdcefbhlchkhipcnnbkcmicncehk_22_314.crx3

This crx is gndmhdcefbhlchkhipcnnbkcmicncehk 22.314 from Chrome Web Store.

This is a version of Assessment Assistant extension that contains verified contents
only in the header of crx. Is used in ComponentUnpackerTest.UnpackWithVerifiedContents
test to check that decompressing of verified contents from header, in case we don't have
a verified contents json inside of crx, is happening correctly. Not having this feature
in ComponentUnpacker previously cased bugs, see [b/242555513](http://b/242555513).
