This directory contains the origin matcher. This struct can be used to share matching rules over mojo
using [scheme_host_port_matcher.h](../../net/base/scheme_host_port_matcher.h).

This should remain as clean as possible from dependencies because
`//content` depends on this component.