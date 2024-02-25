# Hotspot config

This directory contains the implementation of the hotspot service,
whose mojo interface lives in
chromeos/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.

# APIs

[cros_hotspot_config](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/services/hotspot_config/cros_hotspot_config.h;l=6;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;bpv=0;bpt=0) exposes methods to perform following operations:
1. Enable Hotspot
2. Disable Hotspot
3. Set Hotspot Config
4. Get Hotspot Info

In addition to above, couple of methods are exposed to add observers for listening to changes in
hotspot info (connected client counts, config changes) and state changes (enabled, disabled)

# Clients

ChromeOS UI pages, [hotspot_subpage.ts](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/internet_page/hotspot_subpage.ts;l=1;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;bpv=1;bpt=0) and [hotspot_summary_item.ts](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/internet_page/hotspot_summary_item.ts;l=1;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;bpv=1;bpt=0?q=hotspot_summary_item.ts&sq=&ss=chromium%2Fchromium%2Fsrc) and Quick Settings classes ([Hotspot detailed view](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/hotspot/hotspot_detailed_view.h;l=1;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;bpv=1;bpt=0), [Hotspot feature pod controller](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/hotspot/hotspot_feature_pod_controller.h;l=1;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;bpv=1;bpt=0) etc) are primary consumers of these APIs.
Any user initiated operations on these pages will result in calls to this API.

# Dependencies

cros_hotspot_config depends on following classes to execute its operations:
1. [Hotspot Controller](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_controller.h;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd)
2. [Hotspot Configuration Handler](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_configuration_handler.h;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;)
3. [Hotspot State Handler](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_state_handler.h;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd)
4. [Hotspot Capabilities Provider](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_capabilities_provider.h;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd)
5. [Hotspot Enabled State Notifier](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/ash/components/network/hotspot_enabled_state_notifier.h;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd)

Hotspot controller is used to perform enable and disable hotspot operations.
This dependency is used by Enable and Disable hotspot APIs.

Hotspot state handler provides information on the current state of hotspot like the active
client count and if the hotspot is enabled or disabled. This dependency is used by the Get hotspot info API.

Hotspot configuration handler is used to perform the get and set operations on hotspot config.
This dependency is used by Get hotspot info and Set hotspot config APIs.

Hotspot capabilities provider is used to fetch capabilities such as the allow_status (indicates
if enabling hotspot is allowed or not) and available Wi-Fi security modes. This dependency is used by the
Get hotspot info API.

Hotspot enabled state notifier is used to send events related to hotspot status. The API which makes use
of this is primarily used by the [hotspot_notifier](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/hotspot/hotspot_notifier.h;drc=dcf55608c196ee07c12747df2dd7bbe7a6a30bdd;bpv=1;bpt=1)
class which is responsible for UI notifications.
