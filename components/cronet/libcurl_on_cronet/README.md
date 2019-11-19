# libcurl_on_cronet - libcurl wrapper on Cronet native API

The libcurl_on_cronet library is a C++ wrapper library implemented via the
Cronet Native C API, providing synchronous behavior for URL requests using
the Chrome Network Stack. It uses the
[libcurl-easy](https://curl.haxx.se/libcurl/c/libcurl-easy.html) C89 interface,
the subset of libcurl's C API that supports synchronous URL transfers, and is
implemented in C++ 14.

## Overview

The Cronet library provides a cross-platform Native API on top of the Chrome
Network Stack, making it possible to use advanced protocols that would
otherwise be difficult for an application to implement from scratch. The base
asynchronous API offered by Cronet requires little overhead on top of the
Chrome Network Stack, optimizing performance. However, it is non-trivial
to get started using it for URL transfers, as it requires implementation of
asynchronous components such as executors and callbacks regardless of whether
the consuming application has or supports asynchronous, multithreaded behavior.

To lower the bar for trying out Cronet, this library supports synchronous URL
requests by implementing the underlying details required by the base
asynchronous API. It defines a minimally functional executor and
callback and handles management of the Cronet Engine and UrlRequest objects,
supporting a subset of the configuration options available in the
libcurl-easy API. This allows developers who are already familiar with libcurl 
to more easily try out Cronet, without having to initially learn a new interface 
and its corresponding workflow.

### Command Line Tool

There are also plans to develop a command line tool that can invoke a subset of
the implemented API functions in the terminal. This would aid in debugging
networking issues or test the functionality of the Chrome Network Stack in an
easily reproducible manner, acting as a lightweight end-to-end integration test
of the stack without the bulk of the rest of the Chromium browser. With a
simpler binary it would be easier to narrow down the possible points of failure
and pinpoint the root cause of a specific networking issue.

This tool uses the same command line flags as the curl command supported by
libcurl, supporting only a smaller subset of curl's available options. This
smaller binary would be easier to debug and maintain while still maintaining
a sense of familiarity, allowing for easy compatibility with the
["copy as cURL"](https://developers.google.com/web/updates/2015/05/replay-a-network-request-in-curl)
Chromium devtools feature.

This library is currently in active development.

## Details

TODO(michelleroxas): Add more description to this as the project details
become more fleshed out.
