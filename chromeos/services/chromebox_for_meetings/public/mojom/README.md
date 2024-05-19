These mojoms are for a service that exists outside of chromium. Implementation
for CfmServiceContext are found in the private CrOS repo.

Note: [cfm_service_manager.mojom] is copied as is from its chromeos version as
such the original file should be edited before changing this file.

Due to the requirement of chrome dependencies individual service implementation
lives in `chrome/browser/ash/chromebox_for_meetings/`.

TODO(https://crbug.com/1403174): These mojoms should be migrated to namespace
ash and should be synced with other repos where these mojoms are located.

Hot-tip: Generate  Js w/

```
$ ninja -C out_${SDK_BOARD}/Release/chromeos/services/chromebox_for_meetings/public/mojom:mojom_js
```
These files can be found in:

`out_${SDK_BOARD}/Release/gen/chromeos/services/chromebox_for_meetings/public/mojom`

[cfm_service_manager.mojom]: https://source.chromium.org/chromium/chromium/src/+/HEAD:chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom
