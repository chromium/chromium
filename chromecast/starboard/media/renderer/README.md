This directory contains the StarboardRenderer class and other helper classes
that implement various parts of the renderer's functionality.

This code is meant to replace CastRenderer and the CMA pipeline, specifically
for use in linux cast builds. It offers several advantages over the CMA
pipeline:

- Fewer buffer copies. CMA passes buffers as raw ptrs; due to the lifetime model
  of buffers in Starboard, we needed to make a copy of each buffer to ensure
  that it was not deleted before Starboard was done using it. Chromium's APIs
  use scoped_refptrs, and we take advantage of that to control buffer lifetimes.
- Fewer type conversions. CMA has its own copies of many media structs (e.g.
  ::chromecast::media::VideoConfig instead of ::media::VideoDecoderConfig). This
  code simply uses the chromium structs.
- Less code (and hopefully simpler code). This code is designed specifically to
  make use of Starboard APIs for decrypting/decoding/rendering.

See https://github.com/youtube/cobalt/tree/main/starboard for details about
Starboard APIs, and see
[here](https://source.chromium.org/chromium/chromium/src/+/main:chromecast/media/cma/;drc=aad3e70bed91bcbcbc983cfa9a8d9a8505c26af2)
for the CMA code that can be replaced by this code.
