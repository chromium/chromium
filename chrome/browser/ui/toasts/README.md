# Toast UI
The toast system allows features to surface a message and an optional action
for users to take. These are short notifications that serve as confirmation of a
successful user action or provide contextual information about a Chrome
initiated action. Each toast must have an icon and a body text; feature teams
can optionally add an action or close button to toasts. Feature teams that want
to introduce a toast will need to introduce a new value to the toast id enum and
create a toast specification that will be registered with the toast registry.

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
void ToastService::RegisterToasts() {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .Build());
}
```

#### Registering a Toast with a Close Button
```
void ToastService::RegisterToasts() {
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
void ToastService::RegisterToasts() {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .AddActionButton(
            button_string_id,
            base::BindRepeating([](BrowserWindowInterface* window) {
              ...
            }))
        .AddCloseButton()
        .Build());
}
```

If the action button callback needs to know which item/toast is being acted on,
you can register a
`base::RepeatingCallback<void(BrowserWindowInterface*, const base::Value&)>` and
supply the identifier via `params.action_button_callback_data = base::Value(id)`
when invoking the toast.

Note: there are a couple restrictions when declaring a toast specification with
an action button:
1. A toast with an action button must also have a "X" close button.
2. A toast with an action button cannot have a three dot menu.

#### Registering a Toast with a Menu
```
void ToastService::RegisterToasts() {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .AddMenu()
        .Build());
}
```
Even though you have registered the toast with the ToastService, when you
trigger the toast, you must remember to pass in a non-null menu model to
populate the menu in `ToastParams` when you trigger your toast.

Also note that toasts with a menu cannot have a "X" close button. If this
behavior is needed, please consult with UX on how to design this in a way
that supports both.

#### Registering a Toast that Persists on Navigation
By default, tab-scoped toasts will disappear when the user navigates to a new
page or switches tabs.

Toasts that persist on navigation are still tab-scoped, meaning they will
disappear if the user switches to a different tab, but they will survive
navigation events (like form submissions) within the same tab. This is
different from global toasts, which are not tied to any specific tab and will
persist across both tab switches and navigations until they are explicitly
closed or time out.

```
void ToastService::RegisterToasts() {
  ...
  toast_registry_->RegisterToast(
    ToastId,
    ToastSpecification::Builder(vector_icon, string_id)
        .SetPersistOnNavigation()
        .Build());
}
```

### 3. Update the Toast Metrics
Toast metrics are automatically recorded using the new `ToastId` and its
corresponding string name from [`GetToastName()`](api/toast_id.cc). Update the
following metrics files to include your new toast:

* **[`tools/metrics/histograms/metadata/toasts/enums.xml`](/tools/metrics/histograms/metadata/toasts/enums.xml)**:
  Add your new `ToastId` entry to the `ToastId` enum (used by
  `Toast.TriggeredToShow` and `Toast.FailedToShow`). The integer value must
  match the value added in [`api/toast_id.h`](api/toast_id.h).
* **[`tools/metrics/histograms/metadata/toasts/histograms.xml`](/tools/metrics/histograms/metadata/toasts/histograms.xml)**:
  Add a new `<variant>` entry under the `ToastName` `<variants>` element (used
  by the `Toast.{ToastName}.Dismissed` histogram). The `name` attribute must
  match the string returned by `GetToastName()` in
  [`api/toast_id.cc`](api/toast_id.cc).
* **[`tools/metrics/actions/actions.xml`](/tools/metrics/actions/actions.xml)**:
  If your toast has an action button or a close button, add a new `<variant>`
  entry (prefixed with `.`, e.g. `.MyToastName`) to the `Toast_Type`
  `<variants>` element (used by `Toast.ActionButtonClicked{Toast_Type}` and
  `Toast.CloseButtonClicked{Toast_Type}`). The suffix must match the string
  returned by `GetToastName()` in [`api/toast_id.cc`](api/toast_id.cc).

### 4. Trigger your Toast
When you want to trigger your toast to show, you will need to retrieve the
[ToastController](toast_controller.h) using
`ToastController::From(browser_window_interface)` and
call `ToastController::MaybeShowToast(ToastParams params)`.

Note: `ToastController::From()` returns null when no controller is registered on
the window, including before the controller is constructed and after it is
destroyed. The window must still be alive when calling `From()`. Production code
creates controllers only for normal and app windows. Check that the controller
exists before attempting to trigger the toast.

#### Triggering a Toast Without Needing to Replace Placeholder Strings
```
ToastController* const toast_controller =
    ToastController::From(browser_window_interface);
if (toast_controller) {
  toast_controller->MaybeShowToast(ToastParams(ToastId));
}
```

#### Triggering a Toast and Replacing Placeholder Strings
```
ToastController* const toast_controller =
    ToastController::From(browser_window_interface);
if (toast_controller) {
  ToastParams params = ToastParams(ToastId);
  params.body_string_replacement_params = {string_1, string_2};
  params.action_button_string_replacement_params = {string_3};
  toast_controller->MaybeShowToast(std::move(params));
}
```

#### Triggering a Toast with a Menu
```
ToastController* const toast_controller =
    ToastController::From(browser_window_interface);
if (toast_controller) {
  std::unique_ptr<MyCustomMenuModel> menu_model = ...
  ToastParams params = ToastParams(ToastId);
  params.menu_model = std::move(menu_model);
  toast_controller->MaybeShowToast(std::move(params));
}
```

#### Triggering a Toast with a Callback on Close
When the toast gets destroyed, the callback will be called.
```
ToastController* const toast_controller =
    ToastController::From(browser_window_interface);
if (toast_controller) {
  ToastParams params = ToastParams(ToastId);
  params.toast_close_callback = base::ScopedClosureRunner(
      base::BindOnce(&MyClass::OnToastClosed,
                     weak_factory_.GetWeakPtr(), extra_variable_1));
  toast_controller->MaybeShowToast(ToastParams(std::move(params)));
}
```

Note: even though you have triggered your toast, there is a chance that users
might not see the toast because another toast immediately triggered after your
toast, thus preempting your toast from being seen by users.

## Ownership and Testing

In `InitPostWindowConstruction()`, `BrowserWindowFeatures` creates and owns both
`ToastService` and `ToastController` for normal and app windows. It creates the
service first, then creates the controller through `UserDataFactory` and calls
`Init()` on it. `ToastService` registers the toast specifications and owns the
`ToastRegistry` passed to the controller. In
`TearDownPreBrowserWindowDestruction()`, the controller is destroyed before the
service while the browser window is still available.
Consumers retrieve the controller through its
[UnownedUserData](../../../../ui/base/unowned_user_data/README.md) registration.

### Unit tests

To test a feature that requests toasts, use
[MockToastController](test/mock_toast_controller.h) with a
`MockBrowserWindowInterface`. The mock controller registers itself on the mock
window, so the feature uses the same `ToastController::From()` lookup as in
production. The mock overrides `MaybeShowToast()`; other methods retain their
real implementations. Testing requests through this mocked method does not
require a real browser window or toast widget.

```cpp
testing::NiceMock<MockBrowserWindowInterface> browser;
testing::StrictMock<MockToastController> controller(&browser);
EXPECT_CALL(controller, MaybeShowToast(testing::_))
    .WillOnce(testing::Return(true));
// Exercise the feature using &browser.
```

The mock window must outlive the controller. Do not register a second controller
on a window that already has one. A default-constructed `MockToastController`
does not register itself on a window. Pass the mock window to its constructor
when testing code that uses `ToastController::From()`. Depend on
`//chrome/browser/ui/toasts:test_support` and
`//chrome/browser/ui/browser_window/test:test_support` for these mocks.

### Browser tests

To replace the controller in a real window, install a factory override before
the browser is created, for example in the test fixture constructor:

```cpp
toast_controller_override_ =
    BrowserWindowFeatures::GetUserDataFactoryForTesting()
        .AddOverrideForTesting(base::BindRepeating(
            [](BrowserWindowInterface& browser) {
              return std::make_unique<testing::NiceMock<MockToastController>>(
                  &browser);
            }));
```

Keep the returned `ui::UserDataFactory::ScopedOverride` as a fixture member.
While the override is installed, each newly created supported window gets its
own mock, owned and initialized by `BrowserWindowFeatures` and accessible through
`ToastController::From()`. Existing windows are not modified. This avoids adding
toast-specific test setters to production classes. If a test also calls
controller methods that access the registry, pass
`ToastService::From(&browser)->toast_registry()` as the mock constructor's second
argument; real UI behavior still requires the corresponding window and view
dependencies.
