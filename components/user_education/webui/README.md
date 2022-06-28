
# WebUI HelpBubble Implementation (Backend)

[Frontend documentation can be found here.](/ui/webui/resources/cr_components/help_bubble/README.md)

Allows a WebUI page to support Polymer-based, blue material design ("Navi")
[HelpBubble](../common/help_bubble.h)s that can be shown in the course of a
[Feature Promo](../common/feature_promo_controller.h) or
[Tutorial](../common/tutorial.h).

## Usage

 * Implement
   [HelpBubbleHandlerFactory](/ui/webui/resources/cr_components/help_bubble/help_bubble.mojom)
   in your [WebUIController](/content/public/browser/web_ui_controller.h).

 * Register your controller interface, e.g. in
   [chrome_browser_interface_binders.cc](/chrome/browser/chrome_browser_interface_binders.cc).

 * Implement the `CreateHelpBubbleHandler()` method to manufacture a
   [HelpBubbleHandler](./help_bubble_handler.h). Assign a unique `identifier` to
   the handler that will be used when determining when your WebUI is visible and
   where to display help bubbles.

   * For chrome browser, it is considered good practice to declare your
     identifiers in
     [browser_element_identifiers.[h|cc]](/chrome/browser/ui/browser_element_identifiers.h)

   * Note that currently, there can only be one identifier and therefore one
     bubble _per WebUI page_ (not per component); this will be expanded in the
     near future.

 * Implement the frontend by following the directions in
   [the relevant documentation](/ui/webui/resources/cr_components/help_bubble/README.md).

 * Create your User Education journey referencing the `identifier` you used
   above.
   
   * Note that you will need to specify "in any context" for any IPH or tutorial
     steps referencing WebUI. Since WebUI can move between windows, they are
     given unique
     [ElementContext](/ui/base/interaction/element_identifier.h#ElementContext)
     values separate from those of the browser window. This may be changed in
     future for WebContents tied to primary or secondary UI (i.e. not in a tab
     or tab-modal dialog that can move between windows).

## Limitations

As described above, we currently only support a single help bubble-bearing
component (that is, a single component that implements
[HelpBubbleMixin](/ui/webui/resources/cr_components/help_bubble/help_bubble_mixin.ts))
per WebUI page, as there is only a single `ElementIdentifier` that maps between
the front- and backend. We will be easing this constraint in the future.

Also, as described above, the context of the `TrackedElement` that is created by
the handler will not match that of the browser it is currently embedded in, as
`WebContents` may migrate between primary application (e.g. browser, PWA)
windows. We will be looking at ways to mitigate this complication in the future.
In the meantime, please use e.g. `FeaturePromoSpecification::InAnyContext(true)`
for IPH that must reference WebUI.

There may be a number of limitations and unsupported options imposed by the
front-end as well, please see
[the relevant documentation](/ui/webui/resources/cr_components/help_bubble/README.md#Limitations)
for more information before attempting to implement your WebUI User Education
journey. 
