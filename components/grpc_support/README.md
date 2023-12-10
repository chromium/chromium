gRPC Support
===

This directory contains the interface and implementation of the API to use
an external network stack from gRPC. The implementation is essentially a thin
wrapper around net::BidirectionalStream. The API specifies that the caller to
gRPC will pass in an opaque binary blob (stream_engine) that can be used to
created binary streams. In Chromium, this binary blob is a
net::URLRequestContextGetter, which is used by grpc_support::BidirectionalStream
to drive a net::BidirectionalStream.

Eventually code inside of Chromium should be able to use gRPC by providing
a net::URLRequestContextGetter.
