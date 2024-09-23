
# WebUI HelpBubble Implementation (Backend)

[Frontend documentation can be found here.](/ui/webui/resources/cr_components/help_bubble/README.md)

Allows a WebUI page to support Polymer/Lit-based, blue material design ("Navi")
[HelpBubble](../common/help_bubble.h)s that can be shown in the course of a
[Feature Promo](../common/feature_promo_controller.h) or
[Tutorial](../common/tutorial.h).

This is done by associating HTML elements in a component with an
[ElementIdentifier](/ui/base/interaction/element_identifier.h) so they can be
referenced by a Tutorial step or a `FeaturePromoSpecification`.

Once elements are linked in this way, their visibility is reported via
[ElementTracker](/ui/base/interaction/element_tracker.h) and can be referenced
for any of the usual purposes (e.g. in tests or "hidden" Tutorial steps) and
not just for the purpose of anchoring a help bubble.

## Usage

 * Implement
   [HelpBubbleHandlerFactory](/ui/webui/resources/cr_components/help_bubble/help_bubble.mojom)
   in your [WebUIController](/content/public/browser/web_ui_controller.h).

 * Register your controller interface, e.g. in
   [chrome_browser_interface_binders.cc](/chrome/browser/chrome_browser_interface_binders.cc).

 * Implement the `CreateHelpBubbleHandler()` method to manufacture a
   [HelpBubbleHandler](./help_bubble_handler.h).

   * Create a new `HelpBubbleHandler` and store it in a `unique_ptr` on each
     call, discarding any previous handler.

     * `CreateHelpBubbleHandler()` should be called exactly once by a WebUI per
       reload.

     * **Never expose a raw pointer to a `HelpBubbleHandler`** as a reload (or a
       spurious call from a compromised WebUI) could trigger a discard.

   * Assign one or more unique `identifiers` that will correspond to the
     element(s) your bubble(s) will attach to. Each will be mapped to an HTML
     element by your WebUI component.

   * For chrome browser, it is considered good practice to declare your
     identifiers in
     [browser_element_identifiers.[h|cc]](/chrome/browser/ui/browser_element_identifiers.h)

 * Implement the frontend by following the directions in
   [the relevant documentation](/ui/webui/resources/cr_components/help_bubble/README.md).

 * Create your User Education journey referencing one or more of the
   `identifiers` you specified above.
   
   * Note that you will need to specify "in any context" for any IPH or tutorial
     steps referencing WebUI. Since WebUI can move between windows, they are
     given unique
     [ElementContext](/ui/base/interaction/element_identifier.h#ElementContext)
     values separate from those of the browser window. This may be changed in
     future for WebContents tied to primary or secondary UI (i.e. not in a tab
     or tab-modal dialog that can move between windows).

## Limitations

As, as described above, the context of the `TrackedElement` that is created by
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
