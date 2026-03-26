# Cronet HttpURLConnection implementation

This directory contains an implementation of [`java.net.HttpURLConnection`](https://developer.android.com/reference/java/net/HttpURLConnection)
backed by Cronet.

Not to be confused with `JavaCronetEngine` (also known as "fallback" or
"platform"), which goes the opposite direction (Cronet API backed by
`java.net.HttpURLConnection`).

*** note
**CAUTION:** calling into the Cronet API (e.g. `CronetEngine`, `UrlRequest`)
from this code **must be done with extreme care**. This is because, when calling
`CronetEngine#openConnection()` on a `CronetEngine` object provided by Google
Play Services, the objects that are returned are instances of classes that come
from Play Services. But Cronet API classes are always provided by the app, not
by Play Services. So, when `urlconnection` calls into Cronet API, it may be
calling into classes whose version does not match the `urlconnection` code. If
one is not careful, this can easily lead to "method not found" errors (or worse)
depending on which version of the Cronet impl Play Services is bundling on a
given device, and which version of the Cronet API the app is bundling. For this
reason calls from this code into Cronet API must be allowlisted in
`api_static_checks`; new allowlist entries must be carefully scrutinized for
safety.
***
