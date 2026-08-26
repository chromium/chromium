# User Education API for WebUI

This folder provides WebUI pages with a Mojo interface that provides a subset of
the User Education functionality provided by
[BrowserUserEducationInterface](/chrome/browser/ui/user_education/browser_user_education_interface.h).

This Mojo interface can be used in both high- and low-trust WebUI.

For general instructions on how to add User Education for your own features,
[go here](/components/user_education/getting-started.md).

## Using the Handler

Have your page's `MojoWebUIController` implement
`user_education::mojom::UserEducationMixedTrustHandlerFactory` and create a
`UserEducationMixedTrustHandler`. An example can be found in
[UserEducationInternalsUI](/chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h).

Set up the appropriate
[interface binders](/chrome/browser/chrome_browser_interface_binders_webui_parts_desktop.cc)
as you would with any other WebUI (note that binding trusted and untrusted
interfaces requires different code).

No additional setup on the browser side is required.

## Using the API

Import the proxy factory (and proxy, if you prefer to store that), and grab the
handler from that. The handler provides the methods you need. There is no client
interface to implement.

```ts
import type {browserProxyFactory as userEducationProxyFactory} from 'PATH/TO/user_education.mojom-webui.js'

  const userEducationHandler = browserProxyFactory.getInstance().handler;
  userEducationHandler.maybeShowNewBadgeFor('MyPromotedFeature').then(
      (shouldShow: boolean) => {
        /* Maybe display a "New" Badge on a visible element. */
      })
```
## Testing

For testing WebUI elements which use the API:
 - A general-use proxy
   [can be found here](/chrome/test/data/webui/test_user_education_mixed_trust_handler.ts).
 - Be sure to include the User Education browserProxyFactory in your WebUI's
   bundled exports if your WebUI is bundled (see below).

### For Bundled WebUI

In my_webui.ts:
```ts
export {browserProxyFactory as userEducationProxyFactory} from '//resources/mojo/components/user_education/webui/user_education.mojom-webui.js';
```

In my_webui_test.ts:
```ts
import {userEducationProxyFactory} from '<path to my_webui.ts>'
import {TestUserEducationMixedTrustHandler} from '//webui-test/test_user_education_mixed_trust_handler.js';

suite('MyWebUITest', () => {
  let userEducationHandler: TestUserEducationMixedTrustHandler;

  setup(() => {
    userEducationHandler = new TestUserEducationMixedTrustHandler();
    userEducationProxyFactory.setInstance({handler: userEducationHandler});
  });

  // Tests go here. TestUserEducationMixedTrustHandler has the standard
  // TestBrowserProxy interface.
});
```

### For Unbundled WebUI

Same as above, except in the test you can import `userEducationProxyFactory`
directly from
`//resources/mojo/components/user_education/webui/user_education.mojom-webui.js`
with no need to export anything.
