# Optimization Guide

The optimization guide component contains code for processing hints and machine
learning models received from the remote Chrome Optimization Guide Service.

Optimization Guide is a layered component
(https://www.chromium.org/developers/design-documents/layered-components-design)
to enable it to be easily used on all platforms.

## Directory structure

* core/: Shared code that does not depend on src/content/

* content/: Driver for the shared code based on the content layer

* internals/: A submodule with code only present in src-internal checkouts.

* proto/: Protobuf definitions

* public/: Mojom interfaces exposed to sandboxed processes

* optimization_guide_internals/: WebUI for chrome://optimization-guide-internals

## Related Directories

In addition to regular consumers of these services, this code has special relationships with code in the following directories:

* Code that embeds this services
  * //chrome/browser/optimization_guide
  * //ios/chrome/browser/optimization_guide
* A utility service this brokers connections to:
  * //services/on_device_model
* WebUI for chrome://on-device-internals
  * chrome/browser/ui/webui/on_device_internals/
