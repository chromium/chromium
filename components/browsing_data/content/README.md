Types in this directory are used to populate a `CookiesTreeModel`;
see //chrome/browser/browsing_data.

## XYZHelper

Instances of this type are used to fully populate a `CookiesTreeModel`
with full details (e.g. origin/size/modified) for different storage
types, e.g. to report storage used by all origins.

When `StartFetching()` is called, a call is made into the relevant
storage context to enumerate usage info - usually, a set of tuples of
(origin, size, last modified). The CookiesTreeModel assembles this
into the tree of nodes used to populate UI.

Some UI also uses this to delete origin data, which again calls into
the storage context.

## CannedXYZHelper

Note that despite the name ("canned"), this is *not* a test-only type.


Subclass of the above. These are created to sparsely populate a
`CookiesTreeModel` on demand by `LocalSharedObjectContainer`, with
only some details (e.g. full details for cookies, but only the usage
of other storage typess).

* `PageSpecificContentSettings` is notified on storage access/blocked.
* It calls into the "canned" helper instance for the storage type.
* The "canned" instance records necessary "pending" info about the access.
* On demand, the "pending" info is used to populate a CookiesTreeModel.

This "pending" info only needs to record the origin for most storage
types.

## MockXYZHelper

Mock class for testing, only.

Adds an `AddXYZSamples()` method that populates the instance with
test data.
