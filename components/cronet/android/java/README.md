# Cronet impl (and shared) code

This directory contains the so-called Cronet "impl" code, which provides
working production implementations of the Cronet API. Currently this includes:

- The "native" CronetEngine implementation backed by `//net`, which is what most
  people refer to as simply "Cronet" and comes with all the bells and whistles.
  Relevant classes are typically identified by a `Cronet` prefix;
- The "fallback" or "Java" lightweight implementation backed by
  `java.net.HttpURLConnection`. This implementation is much smaller and lacks
  many features. Relevant classes are typically identified by a `Java` prefix.

This directory also contains so-called "shared" code, which is shared between
all Cronet packages, *including* the API package.

TODO(crbug.com/40947707): these should really be three distinct directories.

**IMPORTANT:** in most Cronet release channels (e.g. Google-internal, Maven,
AOSP) the API and impl code are expected to always be in sync with each other,
i.e. they come from the same build. That is NOT true, however, when loading
Cronet from Google Play Services. In that case the app will typically bundle a
Cronet API library that is from a different build/version than the Cronet native
impl code provided by Google Play Services on a given device. In practice this
means that **the API code and native impl code must be backwards and forwards
compatible with each other at the Java ABI level**. Note, though, that Google
Play Services only provides *native* Cronet - this is not an issue for the
fallback implementation.
