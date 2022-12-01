// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_FORM_FOREST_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_FORM_FOREST_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill::internal {

// FormForest converts renderer forms into a browser form and vice versa.
//
// A *frame-transcending* form is a form whose logical fields live in different
// frames. The *renderer forms* are the FormData objects as we receive them from
// the renderers of these frames. The *browser form* of a frame-transcending
// form is its root FormData, with all fields of its descendant FormDatas moved
// into the root.
// See ContentAutofillRouter for further details on the terminology and
// motivation.
//
// Consider the following main frame with two frame-transcending forms:
//
//                    +--------------- Frame ---------------+
//                    |                                     |
//             +--- Form-A --+                       +--- Form-C --+
//             |      |      |                       |             |
//          Field-1 Frame Field-4                  Frame         Frame
//                    |                              |             |
//             +--- Form-B --+                     Form-D        Form-E
//             |             |                       |             |
//          Field-2       Field-3                 Field-5        Frame
//                                                                 |
//                                                               Form-F
//                                                                 |
//                                                              Field-6
//
// The renderer forms are form A, B, C, D, E, F.
//
// The browser form of forms A and B has the fields fields 1, 2, 3, 4.
// Converting this browser form back to renderer forms yields Form-A and Form-B.
//
// Analogously, the browser form of forms C, D, E, and F has the fields 5 and 6.
// Converting this browser form back to renderer forms yields forms C, D, E, F.
//
// The three key functions of FormForest are:
// - UpdateTreeOfRendererForm()
// - GetBrowserFormOfRendererForm()
// - GetRendererFormsOfBrowserForm()
//
// UpdateTreeOfRendererForm() incrementally builds up a graph of frames, forms,
// and fields.
//
// This graph is a forest in two (entirely independent) ways:
//
// Firstly, there may be multiple root frames. One reason is the website author
// can disconnect an entire frame subtree from the rest of the frame tree in the
// future using the fencedframes tag and/or disallowdocumentaccess attribute.
// Another reason is that the frame hierarchy emerges gradually and therefore
// some links may be unknown. For example, Form-A might point to a nonexistent
// frame of Form-B because, after Form-A was last parsed, a cross-origin
// navigation happened in Form-B's frame.
//
// Secondly, removing the root frames obtains a forest, where each tree
// corresponds to a frame-transcending form. We call the roots of this forest
// *root forms*. In the example, forms A and C are root forms. This is relevant
// because filling operations happen on the granularity of root forms.
//
// As an invariant, UpdateTreeOfRendererForm() keeps each frame-transcending
// form in a flattened state: fields are stored as children of their root
// forms. The fields are ordered according to pre-order depth-first (DOM order)
// traversal of the original tree. In our example:
//
//                    +--------------- Frame ---------------+
//                    |                                     |
//    +-------+---- Form-A ---+-------+        +-------+- Form-C -+-------+
//    |       |       |       |       |        |       |          |       |
// Field-1 Field-2 Field-3 Field-4  Frame   Field-5 Field-6     Frame   Frame
//                                    |                           |       |
//                                  Form-B                      Form-D  Form-E
//                                                                        |
//                                                                      Frame
//                                                                        |
//                                                                      Form-F
//
// There is no meaningful order between the fields and frames in these flattened
// forms.
//
// GetBrowserFormOfRendererForm(renderer_form) simply retrieves the form node
// of |renderer_form| and returns the root form, along with its field children
// For example, if |renderer_form| is form B, it returns form A with fields 1–4.
//
// GetRendererFormsOfBrowserForm(browser_form) returns the individual renderer
// forms that constitute |browser_form|, with their fields reinstated. For
// example, if |browser_form| has fields 1–4, it returns form A with fields 1
// and 4, and form B with fields 2 and 3.
//
// The node types in the forest always alternate as follows:
// - The root nodes are frames.
// - The children of frames are forms.
// - The children of forms are frames or fields.
// - Fields are leaves. Forms and frames may be leaves.
//
// Frames, forms, and fields are represented as FrameData, FormData, and
// FormFieldData objects respectively. The graph is stored as follows:
// - All FrameData nodes are stored directly in FormForest::frame_datas_.
// - The FrameData -> FormData edges are stored in the FrameData::child_forms
//   vector of FormData objects. That is, each FrameData directly holds its
//   children.
// - The FormData -> FrameData edges are stored in the FormData::child_frames
//   vector of FrameTokens. To retrieve the actual FrameData child, the token
//   is resolved to a LocalFrameToken if necessary (see Resolve()), and then
//   looked up in FormForest::frame_datas_.
// - The FormData -> FormFieldData edges are stored in the FormData::fields
//   vector of FormFieldData objects. As per the aforementioned invariant,
//   fields are only stored in root forms. Each field's original parent can be
//   identified by FormFieldData::host_frame and FormFieldData::host_form_id.
//
// Reasonable usage of FormForest follows this protocol:
// 1. Call any of the functions only for forms and fields which have the
//    following attributes set:
//    - FormData::host_frame
//    - FormData::unique_renderer_id
//    - FormFieldData::host_frame
//    - FormFieldData::unique_renderer_id
//    - FormFieldData::host_form_id
// 2. Call UpdateTreeOfRendererForm(renderer_form) whenever a renderer form is
//    seen to make FormForest aware of the (new or potentially changed) form.
// 3. Call GetBrowserFormOfRendererForm(renderer_form) only after a preceding
//    UpdateTreeOfRendererForm(some_renderer_form) call where |renderer_form|
//    typically is identical to |some_renderer_form|, but technically it
//    suffices if both forms have the same global_id().
// 4. Call GetRendererFormsOfBrowserForm(browser_form) only if |browser_form|
//    was previously returned by GetBrowserFormOfRendererForm(), perhaps with
//    different FormFieldData::value, FormFieldData::is_autofilled.
// Items 1 and 3 are mandatory for FormForest to be memory-safe.
class FormForest {
 public:
  // A FrameData is a frame node in the form tree. Its children are FormData
  // objects.
  struct FrameData {
    // Less-than relation on FrameData objects based on their frame token, to be
    // used by FrameData sets.
    struct CompareByFrameToken {
      using is_transparent = void;
      bool operator()(const std::unique_ptr<FrameData>& f,
                      const std::unique_ptr<FrameData>& g) const {
        return f && g ? f->frame_token < g->frame_token : f.get() < g.get();
      }
      bool operator()(const std::unique_ptr<FrameData>& f,
                      const LocalFrameToken& g) const {
        return f ? f->frame_token < g : true;
      }
      bool operator()(const LocalFrameToken& f,
                      const std::unique_ptr<FrameData>& g) const {
        return g ? f < g->frame_token : false;
      }
    };

    explicit FrameData(LocalFrameToken frame_token);
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;
    ~FrameData();

    // Unique identifier of the frame. This is never null.
    const LocalFrameToken frame_token;
    // List of forms directly contained in the frame, in the same order as the
    // corresponding UpdateTreeOfRendererForm(child_form) calls. Modification of
    // this vector should be kept to a minimum to ensure memory safety. Only
    // UpdateTreeOfRendererForm() should modify it.
    std::vector<FormData> child_forms;
    // Unique identifier of the form in the parent frame that contains this
    // frame. When a parent form can Resolve() a child's FrameToken, it sets
    // itself as the parent of the child frame, even if no form in this frame
    // has been seen yet.
    absl::optional<FormGlobalId> parent_form = absl::nullopt;
    // Pointer to the frame's ContentAutofillDriver. This can be null because an
    // empty FrameData is created when a parent form can Resolve() a child's
    // LocalFrameToken and no form from that child frame has been seen yet.
    // However, if |child_forms| is non-empty, then driver is non-null.
    raw_ptr<ContentAutofillDriver> driver = nullptr;
  };

  FormForest();
  FormForest(const FormForest&) = delete;
  FormForest& operator=(const FormForest&&) = delete;
  ~FormForest();

  // Adds or updates |renderer_form| and |driver| to/in the relevant tree, where
  // |driver| must be the ContentAutofillDriver of `renderer_form.host_frame`.
  void UpdateTreeOfRendererForm(FormData renderer_form,
                                ContentAutofillDriver* driver) {
    UpdateTreeOfRendererForm(&renderer_form, driver);
  }

  // Returns the browser form of |renderer_form|.
  const FormData& GetBrowserFormOfRendererForm(
      const FormData& renderer_form) const;

  struct RendererForms {
    RendererForms();
    RendererForms(RendererForms&&);
    RendererForms& operator=(RendererForms&&);
    ~RendererForms();
    std::vector<FormData> renderer_forms;
    std::vector<FieldGlobalId> safe_fields;
  };

  // Returns the renderer forms of |browser_form| and the fields that are safe
  // to be filled according to the security policy for cross-frame previewing
  // and filling. The security policy depends on |triggered_origin| and
  // |field_type_map|.
  //
  // The function reinstates each field from |browser_form| in the renderer form
  // it originates from. These reinstated fields hold the (possibly autofilled)
  // values from |browser_form|, provided that they are considered safe to fill
  // according to the security policy defined below. The FormFieldData::value of
  // unsafe fields is reset to the empty string.
  //
  // The |triggered_origin| should be the origin of the field from which
  // Autofill was queried.
  // The |field_type_map| should contain the field types of the fields in
  // |browser_form|.
  //
  // There are two modes that determine whether a field is *safe to fill*.
  // By default, a field is safe to fill iff at least one of the conditions
  // (1–3) and additionally condition (4) hold:
  //
  // (1) The field's origin is the |triggered_origin|.
  // (2) The field's origin is the main origin, the field's type in
  //     |field_type_map| is not sensitive (see IsSensitiveFieldType()), and the
  //     policy-controlled feature shared-autofill is enabled in the field's
  //     frame.
  // (3) The |triggered_origin| is the main origin and the policy-controlled
  //     feature shared-autofill is enabled in the field's frame.
  // (4) No frame on the shortest path from the field on which Autofill was
  //     triggered to the field in question, except perhaps the shallowest
  //     frame, is a fenced frame.
  //
  // If the Finch parameter relax_shared_autofill is true, the restriction to
  // the main origin in condition 3 is lifted. Thus, conditions (2) and (3)
  // reduce to the following:
  //
  // (2+3) The policy-controlled feature shared-autofill is enabled in the
  //       field's document.
  //
  // The *origin of a field* is the origin of the frame that contains the
  // corresponding form-control element.
  //
  // The *main origin* is `browser_form.main_frame_origin`.
  //
  // The "allow" attribute of the <iframe> element controls whether the
  // *policy-controlled feature shared-autofill* is enabled in a document
  // (see https://www.w3.org/TR/permissions-policy-1/).
  RendererForms GetRendererFormsOfBrowserForm(
      const FormData& browser_form,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      const;

  // Deletes all forms and fields that originate from |form| and unsets the
  // FrameData::parent_form pointers of all child forms.
  void EraseForm(FormGlobalId form);

  // Deletes all forms and fields that originate from |frame| and unsets the
  // FrameData::parent_form pointers of all child forms.
  void EraseFrame(LocalFrameToken frame);

  // Resets the object to the initial state.
  void Reset() { frame_datas_.clear(); }

  // Returns the set of FrameData nodes of the forest.
  const base::flat_set<std::unique_ptr<FrameData>,
                       FrameData::CompareByFrameToken>&
  frame_datas() const {
    return frame_datas_;
  }

 private:
  friend class FormForestTestApi;

  struct FrameAndForm {
    constexpr explicit operator bool() {
      DCHECK_EQ(!frame, !form);
      return frame && form;
    }

    raw_ptr<FrameData, DanglingUntriaged> frame = nullptr;
    raw_ptr<FormData, DanglingUntriaged> form = nullptr;
  };

  // Resolves a FrameToken |query| from the perspective of |reference| to the
  // globally unique LocalFrameToken. `reference.driver` must be non-null.
  //
  // Frames identify each other using LocalFrameTokens and RemoteFrameTokens.
  // - LocalFrameTokens are globally unique identifiers and hence suitable for
  //   discrimating between frames.
  // - RemoteFrameTokens are not unique and hence unsuitable to discriminate
  //   between frames.
  //
  // Therefore, FormForest works with LocalFrameToken and resolves the
  // RemoteFrameTokens in FormData::child_frames to LocalFrameTokens.
  //
  // From the perspective of a frame F, a frame G is either local or remote:
  // - If G is local, G is hosted by the same render process as F.
  // - If G is remote, G may be hosted by another render process.
  //
  // Suppose F is the parent frame of G. If G is local to F, then F refers to G
  // in its FormData::child_frames by G's LocalFrameToken. Otherwise, if G is
  // remote to F, then F uses a RemoteFrameToken as a placeholder to refer to G
  // in FormData::child_frames.
  //
  // While LocalFrameTokens are unique identifiers at any point in time, they
  // may change when a navigation happens in the frame:
  // - If G is local to F and a navigation causes G's render process to be
  //   swapped so that G becomes remote, G gets a new LocalFrameToken and F will
  //   refer to G by a fresh RemoteFrameToken.
  // - If G is remote to F and a navigation causes G's render process to be
  //   swapped, then F may continue to refer to G by the same RemoteFrameToken
  //   as before even if G's LocalFrameToken has changed.
  // The first example is the reason why UpdateTreeOfRendererForm() may trigger
  // a reparse in a parent frame. The second example is the reason why we do not
  // cache LocalFrameTokens.
  absl::optional<LocalFrameToken> Resolve(const FrameData& reference,
                                          FrameToken query);

  // Returns the FrameData known for |frame|, or creates a new one and returns
  // it, in which case all members but FrameData::host_frame are uninitialized.
  FrameData* GetOrCreateFrameData(LocalFrameToken frame);

  // Returns the FrameData known for |frame|, or null.
  // May be used in const qualified methods if the return value is not mutated.
  FrameData* GetFrameData(LocalFrameToken frame);

  // Returns the FormData known for |form|, or null.
  // The returned value will point to |frame_datas_|, meaning that all fields
  // have been moved to their respective root forms.
  // The |frame_data| must be null or equal to `GetFrameData(form.host_frame)`.
  // Beware of invalidating the returned form pointer by changing its host
  // frame's FrameData::host_forms.
  // May be used in const qualified methods if the return value is not mutated.
  FormData* GetFormData(const FormGlobalId& form,
                        FrameData* frame_data = nullptr);

  // Returns the non-null root frame and form of the tree that contains |form|.
  // Beware of invalidating the returned form pointer by changing its host
  // frame's FrameData::host_forms.
  // May be used in const qualified methods if the return value is not mutated.
  FrameAndForm GetRoot(FormGlobalId form);

  // Helper for EraseFrame() and EraseForm() that removes all fields that
  // originate from |frame_or_form| and unsets FrameData::parent_form pointer of
  // |frame_or_form|'s children. We intentionally iterate over all frames and
  // forms to search for fields from |frame_or_form|. Alternatively, we could
  // limit this to the root form of |frame_or_form|. However, this would rely on
  // |frame_or_form| being erased before its ancestors, since otherwise
  // |frame_or_form| is disconnected from its root already.
  void EraseReferencesTo(
      absl::variant<LocalFrameToken, FormGlobalId> frame_or_form);

  // Adds |renderer_form| and |driver| to the relevant tree, where |driver| must
  // be the ContentAutofillDriver of the |renderer_form|'s FormData::host_frame.
  // Leaves `*renderer_form` in a valid but unspecified state (like after a
  // move). In particular, `*renderer_form` and its members can be reassigned.
  void UpdateTreeOfRendererForm(FormData* renderer_form,
                                ContentAutofillDriver* driver);

  // The URL of a main frame managed by the FormForest.
  // TODO(crbug.com/1240247): Remove and make Resolve() static.
  std::string MainUrlForDebugging() const;

  // The frame managed by the FormForest that was last passed to
  // UpdateTreeOfRendererForm().
  // TODO(crbug.com/1240247): Remove and make Resolve() static.
  content::GlobalRenderFrameHostId some_rfh_for_debugging_;

  // The FrameData nodes of the forest.
  // The members FrameData::frame_token must not be mutated.
  // Note that since the elements are (smart) pointers, they are not invalidated
  // when the set is resized (unlike pointers or references to the elements).
  base::flat_set<std::unique_ptr<FrameData>, FrameData::CompareByFrameToken>
      frame_datas_;
};

}  // namespace autofill::internal

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_FORM_FOREST_H_
