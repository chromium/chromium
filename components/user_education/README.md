# User Education component library

This library contains the code that (along with
[Feature Engagement](/components/feature_engagement/README.md)) will allow you
to implement **In-Product-Help (IPH)** and **Tutorials** in any framework.

The following libraries are available:
 * [common](./common) - contains platform- and framework-agnostic APIs for
   working with [HelpBubbles](./common/help_bubble.h),
   [IPH](./common/feature_promo_specification.h), and
   [Tutorials](./common/tutorial.h).
 * [views](./views) - contains code required to display a `HelpBubble` in a
   Views-based UI.
 * [webui](./webui/README.md) - contains code required to display a `HelpBubble`
   on a WebUI surface.

The Chrome Browser already builds-in the necessary support for help bubbles
attached to/embedded in Views, WebUI, and Mac-native context menus. You may
refer to
[browser_user_education_service](/chrome/browser/ui/views/user_education/browser_user_education_service.h)
for an example that could be extended to other (especially Views-based)
platforms.