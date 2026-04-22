The files in this directory came from downloading a test extension from the
webstore* that had properly signed verified_contents.json file, taking all
extension files (including _metadata/verified_contents.json and
_metadata/computed_hashes.json) from the chromium profile's Extension/
directory and putting them into source.zip file.

This extension has files that have interesting sizes w.r.t content
verifier's hash block size (4096 bytes):
1024.js
4096.js
8192.js
8191.js
8193.js

* https://chrome.google.com/webstore/detail/dlefkgcbefcjoiheimkdkkhdcejpbgda
