This directory contains NoStatePrefetch implementation.

Currently NoStatePrefetch can only be triggered by `<link rel="prerender">`.
This feature call `StartPrefetchingFromLinkRelPrerender()` on
`NoStatePrefetchManager` to start prefetching.
