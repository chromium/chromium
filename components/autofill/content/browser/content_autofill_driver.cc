// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include "base/barrier_callback.h"
#include "base/functional/callback.h"
#include "components/autofill/content/browser/bad_message.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/signatures.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/origin.h"

namespace autofill {

namespace {

template <typename T, typename... Ts>
concept AnyOf =
    (std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<Ts>> || ...);

// Lift() elevates data from the a renderer process to the browser process.
// Every data received from a renderer should immediately be lifted.
//
// Lifting includes, for example, setting security-critical data like frame
// tokens and origins of Form[Field]Data. Another example is transforming
// coordinates from the originating frame's space to the top-level frame.

// No-op: add types to the `requires` clause below as necessary.
template <typename T>
  requires(std::is_scalar_v<std::remove_cvref_t<T>> ||
           AnyOf<T,
                 bool,
                 AutofillDriverRouter::RoutedCallback<>,
                 base::TimeTicks,
                 std::u16string>)
T&& Lift(ContentAutofillDriver& source, T&& x) {
  return std::forward<T>(x);
}

FormData Lift(ContentAutofillDriver& source, FormData form) {
  content::RenderFrameHost& rfh = *source.render_frame_host();
  form.set_host_frame(source.GetFrameToken());
  form.set_main_frame_origin(rfh.GetMainFrame()->GetLastCommittedOrigin());
  form.set_url([&] {
    // GetLastCommittedURL() doesn't include URL updates due to
    // document.open() and so it might be about:blank or about:srcdoc. In this
    // case fallback to GetLastCommittedOrigin(). See crbug.com/1209270.
    GURL url = StripAuthAndParams(rfh.GetLastCommittedURL());
    if (url.SchemeIs(url::kAboutScheme)) {
      url = StripAuthAndParams(rfh.GetLastCommittedOrigin().GetURL());
    }
    return url;
  }());

  // The form signature must be calculated after setting FormData::url.
  FormSignature signature = CalculateFormSignature(form);
  std::vector<FormFieldData> fields = form.ExtractFields();
  for (FormFieldData& field : fields) {
    field.set_host_frame(form.host_frame());
    field.set_host_form_id(form.renderer_id());
    field.set_host_form_signature(signature);
    field.set_origin(rfh.GetLastCommittedOrigin());
    if (content::RenderWidgetHostView* view = rfh.GetView()) {
      gfx::RectF r = field.bounds();
      r.set_origin(view->TransformPointToRootCoordSpaceF(r.origin()));
      field.set_bounds(r);
    }
  }
  form.set_fields(std::move(fields));
  return form;
}

FormGlobalId Lift(ContentAutofillDriver& source, FormRendererId id) {
  return FormGlobalId(source.GetFrameToken(), id);
}

FieldGlobalId Lift(ContentAutofillDriver& source, FieldRendererId id) {
  return FieldGlobalId(source.GetFrameToken(), id);
}

template <typename T>
auto Lift(ContentAutofillDriver& source, const std::optional<T>& x) {
  std::optional<std::remove_cvref_t<decltype(Lift(source, *x))>> y;
  if (x) {
    y.emplace(Lift(source, *x));
  }
  return y;
}

template <typename T>
auto Lift(ContentAutofillDriver& source, const std::vector<T>& xs) {
  std::vector<std::remove_cvref_t<decltype(Lift(source, xs.front()))>> ys;
  ys.reserve(xs.size());
  for (const auto& x : xs) {
    ys.push_back(Lift(source, x));
  }
  return ys;
}

gfx::Rect Lift(ContentAutofillDriver& source, gfx::Rect r) {
  if (content::RenderWidgetHostView* view =
          source.render_frame_host()->GetView()) {
    r.set_origin(view->TransformPointToRootCoordSpace(r.origin()));
  }
  return r;
}

template <typename... Args>
base::OnceCallback<void(Args...)> Lift(ContentAutofillDriver& source,
                                       base::OnceCallback<void(Args...)> cb) {
  return base::BindOnce(
      [](raw_ref<ContentAutofillDriver> source,
         base::OnceCallback<void(Args...)> cb, Args... args) {
        std::move(cb).Run(Lift(*source, std::forward<Args>(args))...);
      },
      raw_ref(source), std::move(cb));
}

// WithNewVersion() bumps the FormData::version of each form. This should be
// called for every browser form before it enters AutofillManager so that
// AutofillManager can distinguish newer and older forms.
//
// TODO(crbug.com/40144964): Remove once FormData objects aren't stored
// globally anymore.

// No-op: add types to the `requires` clause below as necessary.
template <typename T>
  requires(std::is_scalar_v<std::remove_cvref_t<T>> ||
           AnyOf<T,
                 bool,
                 FieldGlobalId,
                 base::TimeTicks,
                 gfx::Rect,
                 std::u16string,
                 std::vector<FormGlobalId>>)
T&& WithNewVersion(T&& x) {
  return std::forward<T>(x);
}

auto& WithNewVersion(const FormData& browser_form) {
  static FormVersion version_counter;
  ++*version_counter;
  // This const_cast is a hack to avoid additional copies. It's OK because the
  // FormData is owned by AutofillDriverRouter, FormData::version is written
  // only here and read only in AutofillManager.
  const_cast<FormData&>(browser_form).set_version(version_counter);
  return browser_form;
}

auto& WithNewVersion(const std::optional<FormData>& browser_form) {
  if (browser_form) {
    WithNewVersion(*browser_form);
  }
  return browser_form;
}

auto& WithNewVersion(const std::vector<FormData>& browser_forms) {
  for (const FormData& form : browser_forms) {
    WithNewVersion(form);
  }
  return browser_forms;
}

template <typename... Args>
base::OnceCallback<void(Args...)> WithNewVersion(
    base::OnceCallback<void(Args...)> cb) {
  return base::BindOnce(
      [](base::OnceCallback<void(Args...)> cb, Args... args) {
        std::move(cb).Run(WithNewVersion(std::forward<Args>(args))...);
      },
      std::move(cb));
}

// Routes an event from the browser to one or multiple AutofillAgents.
//
// Routing converts values and types: for example, browser forms become renderer
// forms, and FieldGlobalIds become FieldRendererIds.
template <typename R,
          typename... RouterArgs,
          typename... AgentArgs,
          typename... ActualArgs>
R RouteToAgent(AutofillDriverRouter& router,
               R (AutofillDriverRouter::*router_fun)(
                   AutofillDriverRouter::RoutedCallback<AgentArgs...>,
                   RouterArgs...),
               void (mojom::AutofillAgent::*agent_fun)(AgentArgs...),
               ActualArgs&&... args) {
  return (router.*router_fun)(
      [&agent_fun](autofill::AutofillDriver& target, AgentArgs... args) {
        if (!target.IsActive()) {
          // We early-return rather than crashing or killing the renderer
          // because Autofill might want to communicate with a frame that just
          // became inactive due to race conditions. See crbug.com/345195973.
          LOG(WARNING) << "Skipped Autofill message for inactive frame";
          return;
        }
        mojom::AutofillAgent& agent =
            *static_cast<ContentAutofillDriver&>(target).GetAutofillAgent();
        (agent.*agent_fun)(std::forward<AgentArgs>(args)...);
      },
      std::forward<ActualArgs>(args)...);
}

// Routes an event from the renderer to one or multiple AutofillManagers.
//
// Routing converts values and types: for example, renderer forms become browser
// forms, and FieldRendererIds become FieldGlobalIds. Additionally, this
// function takes care of some necessary pre- and postprocessing: for example,
// it sets FormData::frame_token and transforms graphical coordinates, and bumps
// the browser form's FormData::version.
template <typename... RouterArgs,
          typename... ManagerArgs,
          typename... ActualArgs>
void RouteToManager(ContentAutofillDriver& source,
                    AutofillDriverRouter& router,
                    void (AutofillDriverRouter::*router_fun)(
                        AutofillDriverRouter::RoutedCallback<ManagerArgs...>,
                        autofill::AutofillDriver& source,
                        RouterArgs...),
                    void (AutofillManager::*manager_fun)(ManagerArgs...),
                    ActualArgs&&... args) {
  if (!bad_message::CheckArgs(args...) ||
      !bad_message::CheckFrameNotPrerendering(source.render_frame_host())) {
    return;
  }
  return (router.*router_fun)(
      [&manager_fun](autofill::AutofillDriver& target, ManagerArgs... args) {
        AutofillManager& manager = target.GetAutofillManager();
        (manager.*
         manager_fun)(WithNewVersion(std::forward<ManagerArgs>(args))...);
      },
      source, Lift(source, std::forward<ActualArgs>(args))...);
}

}  // namespace

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    ContentAutofillDriverFactory* owner)
    : render_frame_host_(*render_frame_host), owner_(*owner) {
  autofill_manager_ = GetAutofillClient().CreateManager(/*pass_key=*/{}, *this);
}

ContentAutofillDriver::~ContentAutofillDriver() {
  owner_->router().UnregisterDriver(*this, /*driver_is_dying=*/true);
}

void ContentAutofillDriver::Reset(ContentAutofillDriverFactoryPassKey) {
  owner_->router().UnregisterDriver(*this, /*driver_is_dying=*/false);
}

void ContentAutofillDriver::TriggerFormExtractionInDriverFrame(
    AutofillDriverRouterAndFormForestPassKey pass_key) {
  if (!IsActive()) {
    LOG(WARNING) << "Skipped Autofill message for inactive frame";
    return;
  }
  GetAutofillAgent()->TriggerFormExtraction();
}

void ContentAutofillDriver::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool success)> form_extraction_finished_callback) {
  std::vector<ContentAutofillDriver*> drivers;
  render_frame_host()->GetMainFrame()->ForEachRenderFrameHost(
      [&drivers](content::RenderFrameHost* rfh) {
        if (rfh->IsActive()) {
          if (auto* driver = GetForRenderFrameHost(rfh)) {
            drivers.push_back(driver);
          }
        }
      });
  auto barrier_callback = base::BarrierCallback<bool>(
      drivers.size(),
      base::BindOnce(
          [](base::OnceCallback<void(bool success)>
                 form_extraction_finished_callback,
             const std::vector<bool>& successes) {
            std::move(form_extraction_finished_callback)
                .Run(std::ranges::all_of(successes, std::identity()));
          },
          std::move(form_extraction_finished_callback)));
  for (ContentAutofillDriver* driver : drivers) {
    driver->GetAutofillAgent()->TriggerFormExtractionWithResponse(
        barrier_callback);
  }
}

void ContentAutofillDriver::GetFourDigitCombinationsFromDom(
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  if (!IsActive()) {
    LOG(WARNING) << "Skipped Autofill message for inactive frame";
    std::move(potential_matches).Run({});
    return;
  }
  content::RenderFrameHost* main_rfh = render_frame_host_->GetMainFrame();
  if (auto* main_driver = GetForRenderFrameHost(main_rfh)) {
    main_driver->GetAutofillAgent()
        ->GetPotentialLastFourCombinationsForStandaloneCvc(
            std::move(potential_matches));
  }
}

// static
ContentAutofillDriver* ContentAutofillDriver::GetForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host));
  return factory
             ? factory->DriverForFrame(render_frame_host,
                                       base::PassKey<ContentAutofillDriver>())
             : nullptr;
}

void ContentAutofillDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

LocalFrameToken ContentAutofillDriver::GetFrameToken() const {
  return LocalFrameToken(render_frame_host_->GetFrameToken().value());
}

ContentAutofillDriver* ContentAutofillDriver::GetParent() {
  content::RenderFrameHost* parent_rfh = render_frame_host_->GetParent();
  if (!parent_rfh) {
    return nullptr;
  }
  return GetForRenderFrameHost(parent_rfh);
}

ContentAutofillClient& ContentAutofillDriver::GetAutofillClient() {
  return owner_->client();
}

AutofillManager& ContentAutofillDriver::GetAutofillManager() {
  return *autofill_manager_;
}

std::optional<LocalFrameToken> ContentAutofillDriver::Resolve(
    FrameToken query) {
  if (absl::holds_alternative<LocalFrameToken>(query)) {
    return absl::get<LocalFrameToken>(query);
  }
  DCHECK(absl::holds_alternative<RemoteFrameToken>(query));
  content::RenderProcessHost* rph = render_frame_host_->GetProcess();
  blink::RemoteFrameToken blink_remote_token(
      absl::get<RemoteFrameToken>(query).value());
  content::RenderFrameHost* remote_rfh =
      content::RenderFrameHost::FromPlaceholderToken(rph->GetID(),
                                                     blink_remote_token);
  if (!remote_rfh) {
    return std::nullopt;
  }
  return LocalFrameToken(remote_rfh->GetFrameToken().value());
}

bool ContentAutofillDriver::IsActive() const {
  return render_frame_host_->IsActive();
}

bool ContentAutofillDriver::IsInAnyMainFrame() const {
  return render_frame_host_->GetMainFrame() == render_frame_host();
}

bool ContentAutofillDriver::HasSharedAutofillPermission() const {
  return render_frame_host_->IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature::kSharedAutofill);
}

bool ContentAutofillDriver::CanShowAutofillUi() const {
  // Don't show AutofillUi for inactive RenderFrameHost. Here it is safe to
  // ignore the calls from inactive RFH as the renderer is not expecting a reply
  // and it doesn't lead to browser-renderer consistency issues.
  return render_frame_host_->IsActive();
}

std::optional<net::IsolationInfo> ContentAutofillDriver::GetIsolationInfo() {
  return render_frame_host_->GetIsolationInfoForSubresources();
}

base::flat_set<FieldGlobalId> ContentAutofillDriver::ApplyFormAction(
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    base::span<const FormFieldData> data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, FieldType>& field_type_map) {
  // If this driver is active, then its main frame is identical to the main
  // frame at the time the form was received from a renderer and their origins
  // are the same.
  url::Origin main_origin = [&] {
    if (auto* main_rfh = render_frame_host_->GetMainFrame();
        main_rfh->IsInPrimaryMainFrame()) {
      return main_rfh->GetLastCommittedOrigin();
    } else {
      return url::Origin();
    }
  }();
  return RouteToAgent(router(), &AutofillDriverRouter::ApplyFormAction,
                      &mojom::AutofillAgent::ApplyFieldsAction, action_type,
                      action_persistence, data, main_origin, triggered_origin,
                      field_type_map);
}

void ContentAutofillDriver::ApplyFieldAction(
    mojom::FieldActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FieldGlobalId& field_id,
    const std::u16string& value) {
  RouteToAgent(router(), &AutofillDriverRouter::ApplyFieldAction,
               &mojom::AutofillAgent::ApplyFieldAction, action_type,
               action_persistence, field_id, value);
}

void ContentAutofillDriver::ExtractForm(FormGlobalId form_id,
                                        BrowserFormHandler final_handler) {
  if (!IsActive()) {
    LOG(WARNING) << "Skipped Autofill message for inactive frame";
    std::move(final_handler).Run(nullptr, std::nullopt);
    return;
  }
  router().ExtractForm(
      [](autofill::AutofillDriver& request_target, FormRendererId form_id,
         AutofillDriverRouter::RendererFormHandler route_response) {
        auto& source = static_cast<ContentAutofillDriver&>(request_target);
        source.GetAutofillAgent()->ExtractForm(
            form_id, Lift(source, std::move(route_response)));
      },
      form_id, WithNewVersion(std::move(final_handler)));
}

void ContentAutofillDriver::SendTypePredictionsToRenderer(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  // TODO(crbug.com/40753022) Send the FormDataPredictions object only if the
  // debugging flag is enabled.
  RouteToAgent(router(), &AutofillDriverRouter::SendTypePredictionsToRenderer,
               &mojom::AutofillAgent::FieldTypePredictionsAvailable,
               type_predictions);
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const FieldGlobalId& field_id,
    const std::u16string& value) {
  RouteToAgent(
      router(), &AutofillDriverRouter::RendererShouldAcceptDataListSuggestion,
      &mojom::AutofillAgent::AcceptDataListSuggestion, field_id, value);
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  RouteToAgent(router(),
               &AutofillDriverRouter::RendererShouldClearPreviewedForm,
               &mojom::AutofillAgent::ClearPreviewedForm);
}

void ContentAutofillDriver::RendererShouldTriggerSuggestions(
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source) {
  RouteToAgent(
      router(), &AutofillDriverRouter::RendererShouldTriggerSuggestions,
      &mojom::AutofillAgent::TriggerSuggestions, field_id, trigger_source);
}

void ContentAutofillDriver::RendererShouldSetSuggestionAvailability(
    const FieldGlobalId& field_id,
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  RouteToAgent(router(),
               &AutofillDriverRouter::RendererShouldSetSuggestionAvailability,
               &mojom::AutofillAgent::SetSuggestionAvailability, field_id,
               suggestion_availability);
}

void ContentAutofillDriver::FormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormRendererId>& removed_forms) {
  RouteToManager(*this, router(), &AutofillDriverRouter::FormsSeen,
                 &AutofillManager::OnFormsSeen, updated_forms, removed_forms);
}

void ContentAutofillDriver::FormSubmitted(
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource submission_source) {
  RouteToManager(*this, router(), &AutofillDriverRouter::FormSubmitted,
                 &AutofillManager::OnFormSubmitted, form, known_success,
                 submission_source);
}

void ContentAutofillDriver::CaretMovedInFormField(
    const FormData& form,
    FieldRendererId field_id,
    const gfx::Rect& caret_bounds) {
  RouteToManager(*this, router(), &AutofillDriverRouter::CaretMovedInFormField,
                 &AutofillManager::OnCaretMovedInFormField, form, field_id,
                 caret_bounds);
}

void ContentAutofillDriver::TextFieldDidChange(const FormData& form,
                                               FieldRendererId field_id,
                                               base::TimeTicks timestamp) {
  RouteToManager(*this, router(), &AutofillDriverRouter::TextFieldDidChange,
                 &AutofillManager::OnTextFieldDidChange, form, field_id,
                 timestamp);
}

void ContentAutofillDriver::TextFieldDidScroll(const FormData& form,
                                               FieldRendererId field_id) {
  RouteToManager(*this, router(), &AutofillDriverRouter::TextFieldDidScroll,
                 &AutofillManager::OnTextFieldDidScroll, form, field_id);
}

void ContentAutofillDriver::SelectControlDidChange(const FormData& form,
                                                   FieldRendererId field_id) {
  RouteToManager(*this, router(), &AutofillDriverRouter::SelectControlDidChange,
                 &AutofillManager::OnSelectControlDidChange, form, field_id);
}

void ContentAutofillDriver::AskForValuesToFill(
    const FormData& form,
    FieldRendererId field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  RouteToManager(*this, router(), &AutofillDriverRouter::AskForValuesToFill,
                 &AutofillManager::OnAskForValuesToFill, form, field_id,
                 caret_bounds, trigger_source);
}

void ContentAutofillDriver::HidePopup() {
  RouteToManager(*this, router(), &AutofillDriverRouter::HidePopup,
                 &AutofillManager::OnHidePopup);
}

void ContentAutofillDriver::FocusOnNonFormField() {
  RouteToManager(*this, router(), &AutofillDriverRouter::FocusOnNonFormField,
                 &AutofillManager::OnFocusOnNonFormField);
}

void ContentAutofillDriver::FocusOnFormField(const FormData& form,
                                             FieldRendererId field_id) {
  auto focus_no_longer_on_form = [](autofill::AutofillDriver& target) {
    target.GetAutofillManager().OnFocusOnNonFormField();
  };
  RouteToManager(
      *this, router(), &AutofillDriverRouter::FocusOnFormField,
      &AutofillManager::OnFocusOnFormField, form, field_id,
      AutofillDriverRouter::RoutedCallback<>(focus_no_longer_on_form));
}

void ContentAutofillDriver::DidFillAutofillFormData(const FormData& form,
                                                    base::TimeTicks timestamp) {
  RouteToManager(*this, router(),
                 &AutofillDriverRouter::DidFillAutofillFormData,
                 &AutofillManager::OnDidFillAutofillFormData, form, timestamp);
}

void ContentAutofillDriver::DidEndTextFieldEditing() {
  RouteToManager(*this, router(), &AutofillDriverRouter::DidEndTextFieldEditing,
                 &AutofillManager::OnDidEndTextFieldEditing);
}

void ContentAutofillDriver::SelectFieldOptionsDidChange(const FormData& form) {
  RouteToManager(*this, router(),
                 &AutofillDriverRouter::SelectFieldOptionsDidChange,
                 &AutofillManager::OnSelectFieldOptionsDidChange, form);
}

void ContentAutofillDriver::JavaScriptChangedAutofilledValue(
    const FormData& form,
    FieldRendererId field_id,
    const std::u16string& old_value,
    bool formatting_only) {
  RouteToManager(*this, router(),
                 &AutofillDriverRouter::JavaScriptChangedAutofilledValue,
                 &AutofillManager::OnJavaScriptChangedAutofilledValue, form,
                 field_id, old_value, formatting_only);
}

const mojo::AssociatedRemote<mojom::AutofillAgent>&
ContentAutofillDriver::GetAutofillAgent() {
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!autofill_agent_) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &autofill_agent_);
  }
  return autofill_agent_;
}

void ContentAutofillDriver::LiftForTest(FormData& form) {
  form = Lift(*this, form);
}

AutofillDriverRouter& ContentAutofillDriver::router() {
  return owner_->router();
}

}  // namespace autofill
