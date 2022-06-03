# Chrome OS Local Search Service

`LocalSearchService` provides on-device data storage/indexing and flexible search functions via approximate string matching. It supports both in-process and out-of-process clients with the use of a mojo wrapper.

## `Index`
`LocalSearchService` owns one or more `Index`'s, which implement data indexing and search. A client can request to have its own `Index`. Before a client can start using its `Index`, we will need to add a new `IndexId` that will be used by the client as a key when requesting an `Index` via `LocalSearchService::GetIndex`.

### Supported backends
An `Index` could use one of the two backends for storage/indexing and search: linear map (`Backend::kLinearMap`) and inverted index (`Backend::kInvertedIndex`). A client can specify which backend to use by adding a `Backend` parameter to `LocalSearchService::GetIndex`. This is an optional parameter and defaults to `Backend::kLinearMap` if not specified.

If a client's corpus is relatively small, e.g. hundreds of documents and each document contains a few keywords then a linear map should suffice. If the corpus is large and each document is a full article, then an inverted index would be more appropriate: it will parse documents, (optionally) remove stopwords and rank documents via TF-IDF.

### Search parameters

A client can configure `SearchParams` of its `Index`, but this is not necessary because default parameters have been set to optimal values. We suggest you reach out to us before changing the parameters.

## How to use
### In-process clients
An `Index` can be requested as follows
1. Call `LocalSearchServiceFactory::GetForProfile` to obtain a `LocalSearchService` pointer.
2. Call `LocalSearchService::GetIndex` via the pointer above to obtain an `Index` pointer.

Subsequently the client can start adding/updating/deleting data and search. For details, please see `index.h` for all functions and return results.

### Out-of-process clients
If a client is running out-of-process, the client will need to
1. Set up a remote to `mojom::LocalSearchServiceProxy`, have the receiving end bound to its implementation via `LocalSearchServiceProxyFactory::GetForProfile(profile)->BindReceiver`.
2. Set up a remote to `mojom::IndexProxy`, have the receiving end bound to its implementation via `mojom::LocalSearchServiceProxy::GetIndex`.

Functions will be similar to their in-process counterparts, except the calls now will be asynchronous and results will be returned via callbacks. For details, please see `proxy/local_search_service_proxy.mojom`'
