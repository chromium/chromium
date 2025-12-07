# Custom Help Bubbles

A custom help bubble is a UI used specifically for
[feature promos](./feature-promos.md) instead of a normal blue help bubble.
You can use a custom help bubble when you want all of the normal rate-limiting
and contention management of the feature promo system, but you need something
other than a blue bubble.

Note that custom help bubble UI are subject to allowlisting and UI review;
custom help bubble UI must conform to the normal requirements of help bubbles:
 - Must have an (X) button in the upper right or a prominent dismiss button
 - Must respond to ESC with the same effect as (X)
 - Must meet all other accessibility requirements

Note that custom help bubble UI will be focused when shown, so take that into
account when designing them, especially for screen reader users.

## Registering Custom Help Bubble IPH

When registering `browser_user_education_service.cc` use
`FeaturePromoSpecification::CreateForCustomUi()`. You'll note that this takes a
`CustomHelpBubbleFactoryCallback` which returns a `CustomHelpBubble`. Most of
the time you won't be directly creating your own CustomHelpBubble (though you
can!) but instead using a utility method to directly wrap your UI.

Which utility method you use to create your `CustomHelpBubbleFactoryCallback`
will depend on whether you are creating a Views BubbleDialog or a WebUI dialog.
If you are not creating one of these supported UI types, you will have to both
create your own UI and your own `CustomHelpBubble` class to wrap it.

## Custom Promo UI Using Views Bubble Dialogs

The simplest way to create a custom UI for your promo is by making your own
bubble dialog.

Here are the steps:

1. Derive your class from `BubbleDialogDelegateView` and also inherit from
   `CustomHelpBubbleUi`.
2. Ensure that any input that should close the dialog instead calls
   `CustomHelpBubbleUi::NotifyUserAction()` with one of the appropriate values
   (depending on the type of action the user took).
   - If you cannot prevent your bubble from closing (e.g. you are using default
     dismiss-on-escape behavior) make sure to call before closing the bubble.
   - If the bubble closes for any reason before `NotifyUserAction()` then an
     "aborted" result will be registered instead of the correct result.
   - Don't be afraid to call `DialogDelegate::SetCancelCallback()` and
     `DialogDelegate::SetAcceptCallback()` to set up some default behavior
     handling.

That's it! There's a ready (if very simple) example in
[TestCustomHelpBubbleView](./test/test_custom_help_bubble_view.h). For layout
examples (including how to get the close button positioned) check out
[HelpBubbleView](./views/help_bubble_view.h).

### Registering

To register a Custom UI feature promo with a Views-based custom bubble, first
follow all of the [normal steps for feature promos](./feature-promos.md), then
bind a `CustomHelpBubbleViewFactoryCallback` and pass it to
`CreateCustomHelpBubbleViewFactoryCallback()`.

Example:

```cpp
  // This could go inline in the call below, but is separated to make the
  // example clearer.
  auto my_custom_help_bubble_view_factory_callback =
      base::BindRepeating(
          [](ui::ElementContext from_context,
             HelpBubbleArrow arrow,
             FeaturePromoSpecification::BuildHelpBubbleParams build_params) {
            return std::make_unique<MyCustomUiBubbleDialogView>(
              build_params.anchor_element->AsA<views::TrackedElementViews>()->view(),
              arrow);
          });

  feature_promo_registry.Register(
      user_education::FeaturePromoSpecification::CreateForCustomUi(
          kIPHMyCustomUIPromoFeature,
          kAnchorElementId,
          CreateCustomHelpBubbleViewFactoryCallback(
              my_custom_help_bubble_view_factory_callback));
  )
```

Note: you can use the `from_context` parameter to locate the browser that the
promo is being launched from, even if `build_params.anchor_element` is in a
different window.

This example assumes that the anchor is a `View`, however you can get creative
if you want to; see
[FloatingWebUIHelpBubbleFactory](./webui/floating_webui_help_bubble_factory.h)
for one of the more sophisticated examples.

For handling of "custom action" IPH-like dialogs,
[see below](#custom-and-follow-up-actions).

## Custom Promo UI Using WebUI Bubble Dialogs

You can create dialogs without the limitations of Views by using WebUI (though
there is significantly more boilerplate involved). When you create a WebUI in
this way, it must be a "Top Chrome" WebUI, which has a few extra requirements so
it can be easily wrapped in a bubble dialog.

On the browser/C++ side:

1. Your controller class must extend `TopChromeWebUIController` and
   `CustomWebUIHelpBubbleController`.
   - You probably want `enable_chrome_send = true` as well.
   - The latter also puts this into the `UserEducation.` histogram bucket for
     Top Chrome WebUI performance metrics.
2. Call `webui::SetupWebUIDataSource()` in the constructor with the appropriate
   parameters.
3. Add `WEB_UI_CONTROLLER_TYPE_DECL()` in the header and
   `WEB_UI_CONTROLLER_TYPE_IMPL(ClassName)` in the source file.
4. Use `DECLARE_TOP_CHROME_WEBUI_CONFIG(ClassName)` to automatically create a
   config for your WebUI.
5. Call `.AddWebUIConfig(ClassNameConfig)` in
   [this file](/chrome/browser/ui/webui/chrome_web_ui_configs.cc).
6. Call or add to the appropriate `RegisterWebUIControllerInterfaceBinder()` in
   [this file](/chrome/browser/chrome_browser_interface_binders_webui.cc).
   - This should include `CustomHelpBubbleHandlerFactory` and any additional
     mojo bindings you need.

On the WebUI/Typescript side:
1. Fetch the `CustomHelpBubbleHandlerInterface` by calling
   `CustomHelpBubbleProxyImpl.getInstance().getHandler()`.
2. When the user interacts with a button or link, call
   `handler.notifyUserAction()` with the appropriate value.
   - Ensure you have a proper (X) close button as well as at least one link or
     action button the user can interact with.

Unlike Views, `CustomWebUIHelpBubbleController` is already a convenience class
that handles a lot of the boilerplate (yes, there's more than what's listed
above).

### Registering

To register a Custom UI feature promo with a WebUI-based custom bubble, first
follow all of the [normal steps for feature promos](./feature-promos.md), then
pass your WebUI's URL to `MakeCustomWebUIHelpBubbleFactoryCallback<T>()`.

Example:

```cpp
  feature_promo_registry.Register(
      user_education::FeaturePromoSpecification::CreateForCustomUi(
          kIPHMyCustomUIPromoFeature,
          kAnchorElementId,
          MakeCustomWebUIHelpBubbleFactoryCallback<
              MyCustomHelpBubbleWebUIController>(kMyWebUIUrl)));
```

For handling of "custom action" IPH-like dialogs,
[see below](#custom-and-follow-up-actions).

## Custom and Follow-Up Actions

There are three ways you can add additional behavior for after your bubble
closes:

1. Bubble triggers some additional action in the browser via a button, link,
   etc. in response to user action.
2. Bubble reports that the user selected "action" as their response, with logic
   provided when the IPH is registered, just as with "custom action"-style IPH.
3. External code decides the bubble should be closed due to user action outside
   the help bubble, and some further action should be taken.

### Option 1: Bubble Takes Action

An example of this would be the user clicking on a link in a WebUI dialog that
opens, say, a settings page.

Alternatively, a button might call some function directly (over a different mojo
interface if in a WebUI) that performs some action in the browser outside the
help bubble.

In all these cases, the bubble code should send `UserAction::kAction`, which
will cause the bubble to close and record the correct IPH result. No further
action is needed.

### Option 2: Custom Action-like Behavior

An example of this would be the bubble having a "Show Me" button that you want
to have open settings, start a tutorial, or something else.

In this case, the bubble can just send `UserAction::kAction`. There is an
optional fourth parameter to `CreateForCustomUi()` that is identical to the
callback in `CreateForCustomAction()`. If the bubble returns `kAction` then
this callback will be called as if this were a custom action IPH.

### Option 3: Close Promo and Continue

As with many other promos, sometimes the user will engage the feature being
promoted outside the promo - for example, the user presses the toolbar or menu
button the IPH is anchored to rather than interacting with the IPH.

In this case, at the very least, as with other IPH, you should call
`BrowserUserEducationInterface::NotifyFeaturePromoFeatureUsed()` (available on
`BrowserWindow`/`BrowserView` as well). However if you want to take additional
action (like highlighting menu elements in the app menu), you can instead call
`CloseFeaturePromoAndContinue()`.

Both of these can close the promo and mark it as dismissed. The latter allows
you to continue the promo indefinitely while the user is having some other
experience (which could be anything). Just remember to release the
`FeaturePromoHandle` when you're done!
