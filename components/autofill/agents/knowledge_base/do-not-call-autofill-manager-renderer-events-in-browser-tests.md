---
name: autofill/do-not-call-autofill-manager-renderer-events-in-browser-tests
description: In browser and interactive UI tests, trigger renderer-initiated Autofill events via page navigation and simulated DOM interaction (or AutofillDriver fallbacks) instead of calling AutofillManager::On* handlers directly.
rubric: testing
severity: should_fix
---

**Rule.** In browser tests (`browser_tests`, `content_browsertests`,
`android_browsertests`, and `interactive_ui_tests`), do not directly call
renderer-initiated event handlers on `AutofillManager` — i.e., methods gated by
`AutofillManager::RendererEventPassKey` (via
`AutofillManagerTestApi::pass_key()`), such as `AutofillManager::OnFormsSeen()`,
`AutofillManager::OnAskForValuesToFill()`, `AutofillManager::OnFormSubmitted()`,
`AutofillManager::OnTextFieldValueChanged()`, or
`AutofillManager::OnFocusOnFormField()`, as well as convenience wrappers like
`TestAutofillManagerTemplate::OnAskForValuesToFillTest()`. Instead:

1. **Strongly preferred — Page navigation, DOM mutation, and simulated user
   input:** Navigate to a real test page or mutate the DOM with
   `content::ExecJs()` so Blink and `AutofillAgent` discover forms and emit
   `FormsSeen` (`OnFormsSeen()`), and simulate user input (such as
   `content::SimulateMouseClickOrTapElementWithId()` in browser tests or
   `AutofillFlow()` in `interactive_ui_tests`) to trigger `AskForValuesToFill`
   (`OnAskForValuesToFill()`), `TextFieldValueChanged`, or `FormSubmitted`.
   Exercising the full Blink, `AutofillAgent`, Mojo, `ContentAutofillDriver`,
   and `AutofillDriverRouter` pipeline is the primary reason to write a browser
   test and should be the default choice.
2. **Acceptable fallback — Browser-to-renderer triggers on `AutofillDriver`:**
   Only when simulated DOM interaction is impractical or flaky (for example due
   to hit-testing or focus races), ask `AutofillAgent` to extract forms and send
   the Mojo IPC via `AutofillDriver::TriggerFormExtractionInAllFrames()` (for
   `OnFormsSeen()`) or `AutofillDriver::RendererShouldTriggerSuggestions()` (for
   `OnAskForValuesToFill()`). Because this skips the Blink trigger, prefer real
   DOM interaction whenever feasible.
3. **Acceptable fallback — `ContentAutofillDriver::renderer_events()`:** When a
   browser or interactive UI test intentionally bypasses the renderer (for
   example to exercise browser-side UI such as popups, context menus, or dialogs
   with a synthetic `FormData`, where a unit test with `TestAutofillDriver` is
   not sufficient), call the `mojom::AutofillDriver` method on
   `ContentAutofillDriver::renderer_events()` (such as `FormsSeen()` or
   `AskForValuesToFill()`). Because this skips Blink and `AutofillAgent`
   extraction, prefer real DOM interaction (or an `AutofillDriver` trigger)
   whenever feasible; if browser-process integration is not needed, prefer a
   unit test with `TestAutofillDriver`.

Direct calls to `AutofillManager`'s `RendererEventPassKey`-gated handlers (such
as `OnFormsSeen()` and `OnAskForValuesToFill()`) and
`OnAskForValuesToFillTest()` are reserved for unit tests using
`TestAutofillDriver`, where no renderer or `ContentAutofillDriver` exists.

**Why.** A browser test's value comes from exercising the real
renderer-to-browser integration. Renderer-initiated handlers on
`AutofillManager` are post-routing entry points gated by
`AutofillManager::RendererEventPassKey`: calling them directly skips Blink and
`AutofillAgent` extraction, skips `ContentAutofillDriver` (which lifts
`FormData` with `host_frame`, `url`, `main_frame_origin`, field origins,
host-form signatures, root-frame coordinate transforms, and an incremented
`FormData::version`), and bypasses `AutofillDriverRouter` (leaving its
`FormForest` uninitialized or stale for frame-transcending forms and subsequent
routed calls such as `AutofillDriver::ApplyFormAction()`).

## Examples

Bad:

```cpp
// Bypasses Blink, AutofillAgent, ContentAutofillDriver, and
// AutofillDriverRouter.
autofill_manager().OnFormsSeen(
    /*updated_forms=*/{synthetic_form},
    /*removed_form_ids=*/{},
    AutofillManagerTestApi::pass_key());
autofill_manager().OnAskForValuesToFill(
    form.ToFormData(), first_field.global_id(), gfx::Rect(),
    AutofillSuggestionTriggerSource::kFormControlElementClicked,
    /*password_request=*/std::nullopt,
    AutofillManagerTestApi::pass_key());
```

Good:

```cpp
// Strongly preferred: Navigate to a real page (or mutate the DOM with
// content::ExecJs()) so Blink and AutofillAgent emit FormsSeen, then simulate
// user input to trigger AskForValuesToFill end-to-end.
ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), form_url));
content::SimulateMouseClickOrTapElementWithId(web_contents(), "field_id");

// Acceptable fallback when DOM hit-testing is impractical or flaky:
autofill_manager().driver().TriggerFormExtractionInAllFrames(base::DoNothing());
autofill_manager().driver().RendererShouldTriggerSuggestions(
    first_field.global_id(),
    AutofillSuggestionTriggerSource::kFormControlElementClicked);

// Acceptable fallback when testing browser UI with synthetic FormData:
content_driver.renderer_events().FormsSeen(
    /*updated_forms=*/{synthetic_form}, /*removed_forms=*/{});
content_driver.renderer_events().AskForValuesToFill(
    synthetic_form, synthetic_form.fields().front().renderer_id(),
    caret_bounds,
    AutofillSuggestionTriggerSource::kFormControlElementClicked,
    /*password_request=*/std::nullopt);
```
