Sync Protocol Style
===================

General guidelines:
* Follow the [Protocol Buffers Style Guide](https://developers.google.com/protocol-buffers/docs/style)
* Follow the [Proto Do's and Don'ts](http://go/protodosdonts) (sorry, Googlers only).
* Maintain local consistency.
* Use proto2 syntax.

Some specific guidelines on top of the general ones above, or just things that often come up:
* Avoid using "explicit version numbers" in your protobuf; instead, have your code test for the existence of a field to determine what to do. Protobufs were written precisely to avoid version-specific logic, and testing for fields is more robust.
* Enum entries should be in ALL_CAPS (different from C++!), and for new code, the first entry should be a `FOO_UNSPECIFIED = 0` one.
* Timestamp fields must specify their epoch and unit in a suffix, in this format: `_[unix|windows]_epoch_[seconds|millis|micros|nanos]`, e.g `creation_time_unix_epoch_millis`.
  * Many existing fields do not follow this format, and specify epoch/unit in a comment instead. Follow the format for all new fields, though!
* Similarly, duration fields must specify their unit as `_[minutes|seconds|millis|...]`.
* Proto changes also require corresponding changes in [proto_visitors.h](https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/proto_visitors.h) and (where appropriate) in [proto_enum_conversions.h/cc](https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/proto_enum_conversions.cc).
* Backwards compatibility: In general, all changes must be fully backwards-compatible - consider that a single user might be running different versions of the browser simultaneously! Also note that Sync supports clients up to a few years old, so deprecating/removing an existing field is typically a multi-year process.
  * As one special case, **renaming** a field **within a specifics message** is generally safe (unless there are special server-side integrations that depend on the name). However, **never** rename fields anywhere outside of specifics.
  * Avoid repurposing existing fields. Instead, add a new field for the new data and deprecate the old field. Also consider adding code to migrate the old field to the new field.
* Adding fields:
  * Any new fields in a proto are unrecognized by older clients. Thus, any such change faces an inherent risk of leading to data loss for a multi-client Sync user when an older client commits. It is recommended for the data type to follow [Protection against data override by old Sync clients][forward-compatibility] for forward-compatibility.
* Deprecating fields:
  * If the field **is** still accessed: Mark it as `[deprecated = true]`. This is the common case, since the browser typically needs to continue supporting the old field for backwards compatibility reasons.
  * If the field **is not** accessed anymore (i.e. no non-ancient clients depend on the field being populated anymore, all migration code has been retired, etc): Remove the field, and add `reserved` entries for both its name and its tag number.
  * **Note**: If your data type is using the [Protection against data override by old Sync clients][forward-compatibility], then even fields that aren't accessed anymore should **not** be removed from the proto definition, since they should still be treated as supported for the purpose of trimming. (Otherwise, the removed fields would forever be carried forward in the data.)

* Deprecating enum values: This is particularly tricky, especially if the default value is not a `FOO_UNSPECIFIED` one (see above). A common pattern is prepending `DEPRECATED_` to the entry name.

For reviewers:
* Be extra careful with protocol changes, especially consider backward and forward compatibility.
* In doubt, loop in a second reviewer from the Sync team.

[forward-compatibility]: https://www.chromium.org/developers/design-documents/sync/old-sync-clients-data-override-protection/
