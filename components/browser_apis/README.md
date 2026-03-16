# Browser APIS

This component exposes a set of WebUI APIs for the browser functionality. These
APIs may be used by any WebUI clients, so that they do not need to define their
own bespoke APIs when implementing a page.

Note that this directory should only contain the API mojom definitions and
typemapping code. It should not contain implementation code. Implementation code
and binding logic should reside in the embedders (e.g. //chrome). At a high
level, this is the general layering of the APIs.

       ------------
       | Clients  |
       ------------
       | API defs |     <-- contained in this component.
    ------------------  <-- abstraction point.
       | API impl |     <-- resides in the embedder.
       -----------
       | Models   |     <-- embedder specific.
       ------------

The APIs should be agnostic of the underlying embedder and should be flexible enough
to present different underlying models. Clients should not have to modify their code
to work with different embedders.

| Platform          | Implementation | Binder |
--------------------|----------------|--
| Desktop Chrome  | [//chrome/browser/ui/tabs/tab_strip_api](../../chrome/browser/ui/tabs/tab_strip_api) | [chrome/browser/ui/webui_browser/webui_browser_ui.cc](../../chrome/browser/ui/webui_browser/webui_browser_ui.cc)

## Mojom API Guidelines
Assume clients are not intimately knowledgeable of a service and that clients *will*
make mistakes when calling an API. Do not subtly swallow the error, make it very
clear to the client when they have made an invalid request.

APIs in the browsers_api component should conform to
[mojo review guidelines](https://docs.google.com/document/d/1Kw4aTuISF7csHnjOpDJGc7JYIjlvOAKRprCTBVWw_E4/edit?tab=t.0#heading=h.84bpc1e9z1bg).
To support multiplatform and multiclients, there are some deviations from the standard.
Below is a table of the adjustments and the rationale behind them. If it is not
mentioned in the table below, follow the standard mojo review guidelines.

* All methods must have a return of result<T, E>. Observer methods are exempt from this
  rule.

   > Consistent error reporting across all interfaces. This allows clients to
   have some basic expectations of the service (e.g.: that clients will be
   notified of failure). It also allows for error propagation from the service
   to the client.

* All enums should have a kUnknown or kUnspecified as the default (0) value.
  > Deterrence against unintentionally setting a semantically meaningful value
    on object initialization. Provision for possible version skew in the future.

* Invalid (not malformed) messages should return a mojo_base.mojom.Error
  error at a minimum. It is advisable to create service-specific errors.
  The service should not crash on invalid requests.
  > Consistent error handling across services. Most of the context is on
    the client side. In a single service, multiclient situation, it can be
    difficult to determine where the invalid request originated from.

* The service may not make any assumptions into the client state and should
  be as stateless as possible. Do not apply the Page/PageHandler pattern for
  (where the Page and PageHandler are entangled pairs) to the browser_api.
   > Promotes reusability and prevents implicit assumptions.

* Use globally identifiable IDs for resources, instead of using remotes to
  identify resources.
  > The processes are at the same trust level. It’s difficult to cross
    reference resources without a stable and globally unique ID. This does
    imply that the service needs to perform ID validation for each invocation.

* Prefer having implementations before reviewing mojo interfaces, but not
  necessary.
  > Browser API service and clients should have the same privilege level. The
    implementation is typically not the interesting part. Having some
    implementation might be helpful to catch obvious mistakes, but different
    platforms will typically have different implementations.

* (Not enforced atm) All interfaces should be defined under a versioned
  namespace.
  > To support future version skew, the version scope helps us determine
    what assumptions can be made about each version. All interface changes
    to a version must be backwards compatible. Breaking changes are allowed
    across versions.
  >
  ```
   // example:
   module tabs_api.mojom.v2;

   interface TabStripService {
      ...
   };
   ```