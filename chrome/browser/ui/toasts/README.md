# Toast UI
The toast system allows features to surface a message and an optional action
for users to take. These are short, ephemeral notifications that serve as
confirmation of a successful user action or provide contextual information
about a Chrome initiated action. Each toast must have an icon and a body text;
feature teams can optionally add an action or close button to toasts. Feature
teams that want to introduce a toast will need to introduce a new value to the
toast id enum and create a toast specification that will be registered with the
toast registry.

## Getting Started
### 1. Add a new entry to the [ToastId enum](api/toast_id.h)
Each toast should have a unique `ToastId` which is used to identify the
toast and retrieve its corresponding specification.

### 2. Register your ToastSpecification in [ToastService::RegisterToasts()](toast_service.cc)
The toast specification allows features to declare what components they want
the toast to have. All toasts must have an icon and body text when shown, but
have the option to add other components such as an action button, close button,
three dot menu, or set the toast to be persistent.

Note: When registering your ToastSpecification, you can use strings with a
placeholder. However, you must remember to pass in any values you want to use
to replace the placeholders with in `ToastParams` when you trigger your toast.

#### Registering a Default Toast
```
void ToastService::RegisterToast(BrowserWindowInterface* interface) {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .Build());
}
```

#### Registering a Toast with a Close Button
```
void ToastService::RegisterToast(BrowserWindowInterface* interface) {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .AddCloseButton()
        .Build());
}
```

#### Registering a Toast with an Action Button
```
void ToastService::RegisterToast(BrowserWindowInterface* interface) {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .AddActionButton(button_string_id, button_closure)
        .AddCloseButton()
        .Build());
}
```

Note: there are a couple restrictions when declaring a toast specification with
an action button:
1. A toast with an action button must also have a "X" close button.
2. A toast with an action button cannot have a three dot menu.

#### Registering Global Toast
```
void ToastService::RegisterToast(BrowserWindowInterface* interface) {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .AddGlobalScope()
        .Build());
}
```

Toasts are not global scoped by default because most toasts are only relevant
for the active page and should hide when the user switches tabs or navigates
to another page.

However, toasts can be globally scoped so that they will not automatically
dismiss due to tab switch or page navigation. The toast will automatically
dismiss after showing for a certain duration similar to tab scoped toasts.

### 3. Trigger your Toast
When you want to trigger your toast to show, you will need to retrieve the
[ToastController](toast_controller.h) through
[BrowserWindowFeatures](../browser_window/public/browser_window_features.h) and
call `ToastController::MaybeShowToast(ToastParams params)`.

Note: the ToastController can be null for certain browser types so you must check
if the controller exists before attempting to trigger the toast.

#### Triggering a Toast Without Needing to Replace Placeholder Strings
```
ToastController* const toast_controller = browser_window_features->toast_controller();
if (toast_controller) {
  toast_controller->MaybeShowToast(ToastParams(ToastId));
}
```

#### Triggering a Toast and Replacing Placeholder Strings
```
ToastController* const toast_controller = browser_window_features->toast_controller();
if (toast_controller) {
  ToastParams params = ToastParams(ToastId);
  params.body_string_replacement_params_ = {string_1, string_2};
  params.action_button_string_replacement_params_ = {string_3};
  toast_controller->MaybeShowToast(ToastParams(std::move(params)));
}
```

Note: even though you have triggered your toast, there is a chance that users
might not see the toast because another toast immediately triggered after your
toast, thus preempting your toast from being seen by users.
