// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_app_service_bridge.h"

#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "components/payments/content/android/byte_buffer_helper.h"
#include "components/payments/content/android/csp_checker_android.h"
#include "components/payments/content/android/jni_payment_app.h"
#include "components/payments/content/android/payment_request_spec.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/url_formatter/elide_url.h"
#include "components/webauthn/android/internal_authenticator_android.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentAppServiceBridge_jni.h"

namespace {
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::payments::mojom::PaymentMethodDataPtr;

void OnCanMakePaymentCalculated(const JavaRef<jobject>& jcallback,
                                bool can_make_payment) {
  Java_PaymentAppServiceCallback_onCanMakePaymentCalculated(
      AttachCurrentThread(), jcallback, can_make_payment);
}

void OnPaymentAppCreated(const JavaRef<jobject>& jcallback,
                         std::unique_ptr<payments::PaymentApp> payment_app) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onPaymentAppCreated(
      env, jcallback,
      payments::JniPaymentApp::Create(env, std::move(payment_app)));
}

void OnPaymentAppCreationError(
    const JavaRef<jobject>& jcallback,
    const std::string& error_message,
    payments::AppCreationFailureReason error_reason) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onPaymentAppCreationError(
      env, jcallback, ConvertUTF8ToJavaString(env, error_message),
      static_cast<jint>(error_reason));
}

void OnDoneCreatingPaymentApps(const JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  Java_PaymentAppServiceCallback_onDoneCreatingPaymentApps(env, jcallback);
}

void SetCanMakePaymentEvenWithoutApps(const JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;
  Java_PaymentAppServiceCallback_setCanMakePaymentEvenWithoutApps(env,
                                                                  jcallback);
}

void SetOptOutOffered(const JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;
  Java_PaymentAppServiceCallback_setOptOutOffered(env, jcallback);
}

}  // namespace

/* static */
void JNI_PaymentAppServiceBridge_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& jrender_frame_host,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jobject>& jpayment_request_spec,
    const JavaParamRef<jstring>& jtwa_package_name,
    // TODO(crbug.com/40182225): Remove jmay_crawl_for_installable_payment_apps,
    // as it is no longer used.
    jboolean jmay_crawl_for_installable_payment_apps,
    jboolean jis_off_the_record,
    jlong native_csp_checker_android,
    const JavaParamRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* render_frame_host =
      content::RenderFrameHost::FromJavaRenderFrameHost(jrender_frame_host);
  if (!render_frame_host)  // The frame is being unloaded.
    return;

  std::string top_origin = ConvertJavaStringToUTF8(jtop_origin);

  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      webdata_services::WebDataServiceWrapperFactory::
          GetPaymentManifestWebDataServiceForBrowserContext(
              render_frame_host->GetBrowserContext(),
              ServiceAccessType::EXPLICIT_ACCESS);

  auto* bridge = payments::PaymentAppServiceBridge::Create(
      std::make_unique<payments::PaymentAppService>(
          render_frame_host->GetBrowserContext()),
      render_frame_host, GURL(top_origin),
      payments::android::PaymentRequestSpec::FromJavaPaymentRequestSpec(
          env, jpayment_request_spec),
      jtwa_package_name ? ConvertJavaStringToUTF8(env, jtwa_package_name) : "",
      web_data_service, jis_off_the_record,
      payments::CSPCheckerAndroid::GetWeakPtr(native_csp_checker_android),
      base::BindOnce(&OnCanMakePaymentCalculated,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&OnPaymentAppCreated,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&OnPaymentAppCreationError,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindOnce(&OnDoneCreatingPaymentApps,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&SetCanMakePaymentEvenWithoutApps,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)),
      base::BindRepeating(&SetOptOutOffered,
                          ScopedJavaGlobalRef<jobject>(env, jcallback)));

  bridge->CreatePaymentApps();
}

namespace payments {
namespace {

// A singleton class to maintain  ownership of PaymentAppServiceBridge objects
// until Remove() is called.
class PaymentAppServiceBridgeStorage {
 public:
  static PaymentAppServiceBridgeStorage* GetInstance() {
    return base::Singleton<PaymentAppServiceBridgeStorage>::get();
  }

  PaymentAppServiceBridge* Add(std::unique_ptr<PaymentAppServiceBridge> owned) {
    DCHECK(owned);
    PaymentAppServiceBridge* key = owned.get();
    owner_[key] = std::move(owned);
    return key;
  }

  void Remove(PaymentAppServiceBridge* owned) {
    size_t number_of_deleted_objects = owner_.erase(owned);
    DCHECK_EQ(1U, number_of_deleted_objects);
  }

 private:
  friend struct base::DefaultSingletonTraits<PaymentAppServiceBridgeStorage>;
  PaymentAppServiceBridgeStorage() = default;
  ~PaymentAppServiceBridgeStorage() = default;

  std::map<PaymentAppServiceBridge*, std::unique_ptr<PaymentAppServiceBridge>>
      owner_;
};

}  // namespace

/* static */
PaymentAppServiceBridge* PaymentAppServiceBridge::Create(
    std::unique_ptr<PaymentAppService> payment_app_service,
    content::RenderFrameHost* render_frame_host,
    const GURL& top_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    const std::string& twa_package_name,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    bool is_off_the_record,
    base::WeakPtr<CSPChecker> csp_checker,
    CanMakePaymentCalculatedCallback can_make_payment_calculated_callback,
    PaymentAppCreatedCallback payment_app_created_callback,
    PaymentAppCreationErrorCallback payment_app_creation_error_callback,
    base::OnceClosure done_creating_payment_apps_callback,
    base::RepeatingClosure set_can_make_payment_even_without_apps_callback,
    base::RepeatingClosure set_opt_out_offered_callback) {
  DCHECK(render_frame_host);
  // Not using std::make_unique, because that requires a public constructor.
  std::unique_ptr<PaymentAppServiceBridge> bridge(new PaymentAppServiceBridge(
      std::move(payment_app_service), render_frame_host, top_origin, spec,
      twa_package_name, std::move(web_data_service), is_off_the_record,
      csp_checker, std::move(can_make_payment_calculated_callback),
      std::move(payment_app_created_callback),
      std::move(payment_app_creation_error_callback),
      std::move(done_creating_payment_apps_callback),
      std::move(set_can_make_payment_even_without_apps_callback),
      std::move(set_opt_out_offered_callback)));
  return PaymentAppServiceBridgeStorage::GetInstance()->Add(std::move(bridge));
}

PaymentAppServiceBridge::~PaymentAppServiceBridge() = default;

void PaymentAppServiceBridge::CreatePaymentApps() {
  payment_app_service_->Create(weak_ptr_factory_.GetWeakPtr());
}

base::WeakPtr<PaymentAppServiceBridge>
PaymentAppServiceBridge::GetWeakPtrForTest() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* PaymentAppServiceBridge::GetWebContents() {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsActive() ? content::WebContents::FromRenderFrameHost(rfh)
                                : nullptr;
}
const GURL& PaymentAppServiceBridge::GetTopOrigin() {
  return top_origin_;
}

const GURL& PaymentAppServiceBridge::GetFrameOrigin() {
  return frame_origin_;
}

const url::Origin& PaymentAppServiceBridge::GetFrameSecurityOrigin() {
  return frame_security_origin_;
}

content::RenderFrameHost* PaymentAppServiceBridge::GetInitiatorRenderFrameHost()
    const {
  return content::RenderFrameHost::FromID(frame_routing_id_);
}

content::GlobalRenderFrameHostId
PaymentAppServiceBridge::GetInitiatorRenderFrameHostId() const {
  return frame_routing_id_;
}

const std::vector<PaymentMethodDataPtr>&
PaymentAppServiceBridge::GetMethodData() const {
  DCHECK(spec_);
  return spec_->method_data();
}

std::unique_ptr<webauthn::InternalAuthenticator>
PaymentAppServiceBridge::CreateInternalAuthenticator() const {
  // This authenticator can be used in a cross-origin iframe only if the
  // top-level frame allowed it with Permissions Policy, e.g., with
  // allow="payment" iframe attribute. The secure payment confirmation dialog
  // displays the top-level origin in its UI before the user can click on the
  // [Verify] button to invoke this authenticator.
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  // Lifetime of the created authenticator is externally managed by the
  // authenticator factory, but is generally tied to the RenderFrame by
  // listening for `RenderFrameDeleted()`. Check `IsRenderFrameLive()` as a
  // safety precaution to ensure that `RenderFrameDeleted()` will be called at
  // some point.
  return rfh && rfh->IsActive() && rfh->IsRenderFrameLive()
             ? std::make_unique<webauthn::InternalAuthenticatorAndroid>(rfh)
             : nullptr;
}

scoped_refptr<PaymentManifestWebDataService>
PaymentAppServiceBridge::GetPaymentManifestWebDataService() const {
  return payment_manifest_web_data_service_;
}

bool PaymentAppServiceBridge::IsOffTheRecord() const {
  return is_off_the_record_;
}

base::WeakPtr<ContentPaymentRequestDelegate>
PaymentAppServiceBridge::GetPaymentRequestDelegate() const {
  // PaymentAppService flow should have short-circuited before this point.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void PaymentAppServiceBridge::ShowProcessingSpinner() {
  // Java UI determines when the show a spinner itself.
}

base::WeakPtr<PaymentRequestSpec> PaymentAppServiceBridge::GetSpec() const {
  return spec_;
}

void PaymentAppServiceBridge::GetTwaPackageName(
    GetTwaPackageNameCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), twa_package_name_));
}

void PaymentAppServiceBridge::OnPaymentAppCreated(
    std::unique_ptr<PaymentApp> app) {
  if (can_make_payment_calculated_callback_)
    std::move(can_make_payment_calculated_callback_).Run(true);

  payment_app_created_callback_.Run(std::move(app));
}

void PaymentAppServiceBridge::OnPaymentAppCreationError(
    const std::string& error_message,
    AppCreationFailureReason error_reason) {
  payment_app_creation_error_callback_.Run(error_message, error_reason);
}

void PaymentAppServiceBridge::OnDoneCreatingPaymentApps() {
  if (number_of_pending_factories_ > 1U) {
    number_of_pending_factories_--;
    return;
  }

  DCHECK_EQ(1U, number_of_pending_factories_);

  if (can_make_payment_calculated_callback_)
    std::move(can_make_payment_calculated_callback_).Run(false);

  std::move(done_creating_payment_apps_callback_).Run();
  PaymentAppServiceBridgeStorage::GetInstance()->Remove(this);
}

void PaymentAppServiceBridge::SetCanMakePaymentEvenWithoutApps() {
  set_can_make_payment_even_without_apps_callback_.Run();
}

base::WeakPtr<CSPChecker> PaymentAppServiceBridge::GetCSPChecker() {
  return csp_checker_;
}

void PaymentAppServiceBridge::SetOptOutOffered() {
  set_opt_out_offered_callback_.Run();
}

std::optional<base::UnguessableToken>
PaymentAppServiceBridge::GetChromeOSTWAInstanceId() const {
  return std::nullopt;
}

PaymentAppServiceBridge::PaymentAppServiceBridge(
    std::unique_ptr<PaymentAppService> payment_app_service,
    content::RenderFrameHost* render_frame_host,
    const GURL& top_origin,
    base::WeakPtr<PaymentRequestSpec> spec,
    const std::string& twa_package_name,
    scoped_refptr<PaymentManifestWebDataService> web_data_service,
    bool is_off_the_record,
    base::WeakPtr<CSPChecker> csp_checker,
    CanMakePaymentCalculatedCallback can_make_payment_calculated_callback,
    PaymentAppCreatedCallback payment_app_created_callback,
    PaymentAppCreationErrorCallback payment_app_creation_error_callback,
    base::OnceClosure done_creating_payment_apps_callback,
    base::RepeatingClosure set_can_make_payment_even_without_apps_callback,
    base::RepeatingClosure set_opt_out_offered_callback)
    : payment_app_service_(std::move(payment_app_service)),
      number_of_pending_factories_(
          payment_app_service_->GetNumberOfFactories()),
      frame_routing_id_(render_frame_host->GetGlobalId()),
      top_origin_(top_origin),
      frame_origin_(url_formatter::FormatUrlForSecurityDisplay(
          render_frame_host->GetLastCommittedURL())),
      frame_security_origin_(render_frame_host->GetLastCommittedOrigin()),
      spec_(spec),
      twa_package_name_(twa_package_name),
      payment_manifest_web_data_service_(web_data_service),
      is_off_the_record_(is_off_the_record),
      csp_checker_(csp_checker),
      can_make_payment_calculated_callback_(
          std::move(can_make_payment_calculated_callback)),
      payment_app_created_callback_(std::move(payment_app_created_callback)),
      payment_app_creation_error_callback_(
          std::move(payment_app_creation_error_callback)),
      done_creating_payment_apps_callback_(
          std::move(done_creating_payment_apps_callback)),
      set_can_make_payment_even_without_apps_callback_(
          std::move(set_can_make_payment_even_without_apps_callback)),
      set_opt_out_offered_callback_(std::move(set_opt_out_offered_callback)) {}

}  // namespace payments
