// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/search_engines/template_url.h"
#include "components/url_formatter/spoof_checks/idna_metrics.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_utils.h"
#include "url/gurl.h"

#if (!BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !BUILDFLAG(IS_IOS)
#define SUPPORT_PEDALS_VECTOR_ICONS
namespace gfx {
struct VectorIcon;
}
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class AutocompleteInput;
class AutocompleteProviderClient;

// Omnibox Actions are additional actions associated with matches. They appear
// in the suggestion button row and are not matches themselves.
//
// Omnibox Pedals are a type of Omnibox Action.
//
// Actions are ref-counted to support a flexible ownership model.
//  - Some actions are ephemeral and specific to the match, and should be
//    destroyed when the match is destroyed, so matches have the only reference.
//  - Some actions (like Pedals) are fixed and expensive to copy, so matches
//    should merely hold one of the references to the action.
// Note: `RefCountedThreadSafe` is used instead of `RefCounted` because
//  AutocompleteMatch instances are passed across thread boundaries to
//  different sequences and they contain `scoped_refptr<OmniboxAction>`.
class OmniboxAction : public base::RefCountedThreadSafe<OmniboxAction> {
 public:
  struct LabelStrings {
    LabelStrings(int id_hint,
                 int id_suggestion_contents,
                 int id_accessibility_suffix,
                 int id_accessibility_hint);
    LabelStrings(std::u16string hint,
                 std::u16string suggestion_contents,
                 std::u16string accessibility_suffix,
                 std::u16string accessibility_hint);
    LabelStrings();
    LabelStrings(const LabelStrings&);
    ~LabelStrings();
    std::u16string hint;
    std::u16string suggestion_contents;
    std::u16string accessibility_suffix;
    std::u16string accessibility_hint;
  };

  // Actions such as Pedals may require various capabilities from an embedding
  // client context and this interface can be used to invert the dependency.
  struct Client {
    // Opens the Sharing Hub as if the "Share this page" airplane button
    // were clicked.
    virtual void OpenSharingHub() = 0;

    // Opens and shows a new incognito browser window.
    virtual void NewIncognitoWindow() = 0;

    // Opens an Incognito clear browsing data dialog.
    virtual void OpenIncognitoClearBrowsingDataDialog() = 0;

    // Closes incognito browser windows.
    virtual void CloseIncognitoWindows() = 0;

    // Presents translation prompt for current tab web contents.
    virtual void PromptPageTranslation() = 0;

    // Opens Journeys in an embedder-specific way. If this returns true, that
    // means that the embedder successfully opened Journeys, and the caller can
    // early exit. If this returns false, the caller should open the WebUI.
    virtual bool OpenJourneys(const std::string& query);
  };

  // ExecutionContext provides the necessary structure for Action
  // execution implementations that potentially vary widely, and
  // references are preferred over pointers for members that are
  // not nullable.  If there's ever a good case for changing to
  // nullable pointers, this can change but for now presence is
  // guaranteed so Actions can use them without needing to check.
  // The other reason to use a context is that it's easier to
  // maintain with lots of Action implementations because the amount
  // of boilerplate required is greatly reduced.
  class ExecutionContext {
   public:
    // Set `match_type` as if the user just typed url verbatim.
    // `destination_url_entered_without_scheme` is used to determine whether
    // navigations typed without a scheme and upgraded to HTTPS should fall back
    // to HTTP. The URL might have been entered without a scheme, but Action
    // destination URLs don't need a fallback so it's fine to pass false here.
    using OpenUrlCallback =
        base::OnceCallback<void(const GURL& destination_url,
                                TemplateURLRef::PostContent* post_content,
                                WindowOpenDisposition disposition,
                                ui::PageTransition transition,
                                AutocompleteMatchType::Type match_type,
                                base::TimeTicks match_selection_timestamp,
                                bool destination_url_entered_without_scheme,
                                bool destination_url_entered_with_http_scheme,
                                const std::u16string&,
                                const AutocompleteMatch&,
                                const AutocompleteMatch&,
                                IDNA2008DeviationCharacter)>;

    ExecutionContext(Client& client,
                     OpenUrlCallback callback,
                     base::TimeTicks match_selection_timestamp,
                     WindowOpenDisposition disposition);
    ~ExecutionContext();
    const raw_ref<Client, FlakyDanglingUntriaged> client_;
    OpenUrlCallback open_url_callback_;
    base::TimeTicks match_selection_timestamp_;
    WindowOpenDisposition disposition_;
  };

  OmniboxAction(LabelStrings strings, GURL url);

  // Provides read access to labels associated with this Action.
  const LabelStrings& GetLabelStrings() const;

  // Returns the destination URL for navigation Actions, Otherwise, returns an
  // empty URL.
  const GURL& getUrl() const { return url_; }

  // Records that the action was shown at index `position` in the popup.
  // `executed` is set to true if the action was also executed by the user.
  virtual void RecordActionShown(size_t position, bool executed) const {}

  // Takes the action associated with this OmniboxAction. Non-navigation
  // actions must override the default, but navigation actions don't need to.
  virtual void Execute(ExecutionContext& context) const;

  // Returns true if this Action is ready to be used now, or false if
  // it does not apply under current conditions. (Example: the UpdateChrome
  // Pedal may not be ready to trigger if no update is available.)
  virtual bool IsReadyToTrigger(const AutocompleteInput& input,
                                const AutocompleteProviderClient& client) const;

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  // Returns the vector icon to represent this Action.
  virtual const gfx::VectorIcon& GetVectorIcon() const;
#endif

  // Estimates RAM usage in bytes for this Action.
  virtual size_t EstimateMemoryUsage() const;

  // Returns an ID used to identify the action.
  virtual OmniboxActionId ActionId() const;

#if BUILDFLAG(IS_ANDROID)
  virtual base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const;

  void RecordActionShown(JNIEnv* env, int position, bool executed) {
    RecordActionShown(position, executed);
  }
#endif

 protected:
  friend class base::RefCountedThreadSafe<OmniboxAction>;
  virtual ~OmniboxAction();

  // Use this for the common case of navigating to a URL.
  void OpenURL(ExecutionContext& context, const GURL& url) const;

  LabelStrings strings_;

  // For navigation Actions, this holds the destination URL. Otherwise, empty.
  GURL url_;

#if BUILDFLAG(IS_ANDROID)
  mutable base::android::ScopedJavaGlobalRef<jobject> j_omnibox_action_;
#endif
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_H_
