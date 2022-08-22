This directory contains an implementation of the
[Open Screen](../../third_party/openscreen) platform API along with utility
functions, using the `//base` and `//net` directories in Chromium.

Any Chromium target that (indirectly) depends on
`//third_party/openscreen/src/platform:api` must also (indirectly) depend on one
of two components provided by this component. In nearly all cases, the dependency
should be in the same `deps` as `//third_party/openscreen/src/platform:api`.
Most external targets should depend on `//components/openscreen_platform`.
Targets that cannot use the Network Service should instead depend on
`//components/openscreen_platform:openscreen_platform_using_net_sockets`, which
uses a `//net`-based implementation of `UdpSocket`. These two targets are
incompatible with each other.

A very small set of intermediary targets that are used by both types of targets
above depend directly on
`//components/openscreen_platform:openscreen_platform_without_sockets` and
push the requirement to depend on one of the two public targets up to the
dependency chain.
