This feature provides the conceptual ability to create and host a surface that
is configured externally, an "external surface". Some off device source will
build and assemble content into opaque data from the client's perspective. When
this data is handed to xfeature, native views will be provided back, allowing a
highly performant and interactive page or part of a page that is also completely
data driven.

Note that this location defines mostly just the interfaces for this feature. The
bulk of the actual implementation is provided by downstream. This means that
changes to these interfaces will likely cause downstream build failures. Please
do not TBR changes through without engaging OWNERS.
