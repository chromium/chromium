# Extension Updater Test Files

## `manifest_v2.json.template`

This is a canned response following the Omaha v4 protocol (detailed in
`//docs/updater/protocol_4.md`). The first line is a Safe JSON prefix, as
described in that document. After that, the template instructs the client to
update to version 2 of the `ogjcoiohnmldgjemafoockdghcjciccf` extension, with
three parameters: $1 is the download URL of the extension, $2 is the hash of
the download, and $3 is the size of the download.
