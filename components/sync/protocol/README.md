Sync Protocol Style
===================

General guidelines:
* Follow the [Protocol Buffers Style Guide](https://developers.google.com/protocol-buffers/docs/style)
* Follow the [Proto Do's and Don'ts](http://go/protodosdonts) (sorry, Googlers only).
* Maintain local consistency.
* Use proto2 syntax.

Some specific guidelines on top of the general ones above, or just things that often come up:
* Enum entries should be in ALL_CAPS (different from C++!), and for new code, the first entry should be a `FOO_UNSPECIFIED = 0` one.
* Timestamp fields must specify their epoch and unit in a suffix, in this format: `_[unix|windows]_epoch_[seconds|millis|micros|nanos]`, e.g `creation_time_unix_epoch_millis`.
  * Many existing fields do not follow this format, and specify epoch/unit in a comment instead. Follow the format for all new fields, though!
* Similarly, duration fields must specify their unit as `_[minutes|seconds|millis|...]`.
* Proto changes also require corresponding changes in [proto_visitors.h](https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/proto_visitors.h) and (where appropriate) in [proto_enum_conversions.h/cc](https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/proto_enum_conversions.cc).
* Backwards compatibility: In general, all changes must be fully backwards-compatible - consider that a single user might be running different versions of the browser simultaneously! Also note that Sync supports clients up to a few years old, so deprecating/removing an existing field is typically a multi-year process.
  * As one special case, **renaming** a field **within a specifics message** is generally safe (unless there are special server-side integrations that depend on the name). However, **never** rename fields anywhere outside of specifics.
* Deprecating fields:
  * If the field **is** still accessed: Mark it as `[deprecated = true]`. This is the common case, since the browser typically needs to continue supporting the old field for backwards compatibility reasons.
  * If the field **is not** accessed anymore (i.e. no non-ancient clients depend on the field being populated anymore, all migration code has been retired, etc): Remove the field, and add `reserved` entries for both its name and its tag number.
* Deprecating enum values: This is particularly tricky, especially if the default value is not a `FOO_UNSPECIFIED` one (see above). A common pattern is prepending `DEPRECATED_` to the entry name.

For reviewers:
* Be extra careful with protocol changes, especially consider backward and forward compatibility.
* In doubt, loop in a second reviewer from the Sync team.
