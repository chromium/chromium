# Network Health

This directory defines a mojo service, network_health.mojom, for requesting a
snapshot of the network health state.

The implementation of the service lives in
chrome/browser/ash/net/network_health/ because it has Chrome dependencies,
e.g. for Captive Portal state.

The mojom lives here so that it is available to other components, e.g. as a
WebUI component in ash/webui/common/resources/network_health/.

