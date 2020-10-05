These mojoms are for a service that exists outside of chromium. Implementation
for CfmServiceContext and CfmServiceRegistry are found in the private CrOS repo.

Note: //chromeos/services/cfm/public/mojom/cfm_service_manager.mojom
is copied as is from its chromeos version as such the original file should be
edited before changing this file.

Due to the requirement of chrome dependencies individual service implementation
lives in `chrome/browser/chromeos/cfm/`.

Hot-tip: Generate  Js w/

```
$ ninja -C out_${BOARD}/Release/ chromeos/services/cfm/public/mojom:mojom_js
```
These files can be found in:

`out_${BOARD}/Release/gen/chromeos/services/cfm/public/mojom`
