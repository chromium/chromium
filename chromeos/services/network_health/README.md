# Network Health

This directory defines several mojo services related to network health and
diagnostics, e.g. for requesting a snapshot of the network health state.

The implementation of the service lives in
chromeos/ash/services/network_health/, as it runs in Ash.

The mojom lives here so that it is available to other components, e.g. as a
WebUI component in ash/webui/common/resources/network_health/.
