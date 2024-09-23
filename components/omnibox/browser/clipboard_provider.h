// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;
class AutocompleteProviderListener;
class ClipboardRecentContent;
enum class ClipboardContentType;

// Autocomplete provider offering content based on the clipboard's content.
class ClipboardProvider : public AutocompleteProvider {
 public:
  ClipboardProvider(AutocompleteProviderClient* client,
                    AutocompleteProviderListener* listener,
                    ClipboardRecentContent* clipboard_content);

  ClipboardProvider(const ClipboardProvider&) = delete;
  ClipboardProvider& operator=(const ClipboardProvider&) = delete;

  // Returns a new AutocompleteMatch clipboard match that will navigate to the
  // given copied url. Used to construct a match later when the URL is not
  // available at match creation time (e.g. iOS 14).
  AutocompleteMatch NewClipboardURLMatch(GURL url);
  // Returns a new AutocompleteMatch clipboard match that will search for the
  // given copied text. Used to construct a match later when the text is not
  // available at match creation time (e.g. iOS 14).
  std::optional<AutocompleteMatch> NewClipboardTextMatch(std::u16string text);

  using ClipboardImageMatchCallback =
      base::OnceCallback<void(std::optional<AutocompleteMatch>)>;
  // Returns a new AutocompleteMatch clipboard match that will search for the
  // given copied image. Used to construct a match later when the image is not
  // available at match creation time (e.g. iOS 14).
  void NewClipboardImageMatch(std::optional<gfx::Image> optional_image,
                              ClipboardImageMatchCallback callback);

  using ClipboardMatchCallback = base::OnceCallback<void()>;
  // Update clipboard match |match| with the current clipboard content.
  void UpdateClipboardMatchWithContent(AutocompleteMatch* match,
                                       ClipboardMatchCallback callback);

  // AutocompleteProvider implementation.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void AddProviderInfo(ProvidersInfo* provider_info) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardProviderTest, MatchesImage);
  FRIEND_TEST_ALL_PREFIXES(ClipboardProviderTest, CreateURLMatchWithContent);
  FRIEND_TEST_ALL_PREFIXES(ClipboardProviderTest, SuppressAfterFirstUsed);
  FRIEND_TEST_ALL_PREFIXES(ClipboardProviderTest, CreateTextMatchWithContent);
  FRIEND_TEST_ALL_PREFIXES(ClipboardProviderTest, CreateImageMatchWithContent);

  ~ClipboardProvider() override;

  // Handle the match created from one of the match creation methods and do
  // extra tracking and match adding.
  void AddCreatedMatchWithTracking(
      const AutocompleteInput& input,
      AutocompleteMatch match,
      const base::TimeDelta clipboard_contents_age);

  // Uses asynchronous clipboard APIs to check which content types have
  // clipboard data without actually accessing the data. If any do, then one
  // clipboard match is created. Calls back to |OnReceiveClipboardContent| with
  // the result.
  void CheckClipboardContent(const AutocompleteInput& input);
  // Called when the clipboard data is returned from the asynchronous call.
  void OnReceiveClipboardContent(const AutocompleteInput& input,
                                 base::TimeDelta clipboard_contents_age,
                                 std::set<ClipboardContentType> matched_types);

  // Checks whether the current template url supports text searches.
  bool TemplateURLSupportsTextSearch();
  // Checks whether the current template url supports image searches.
  bool TemplateURLSupportsImageSearch();

  // Returns a URL match with no URL. This can be used if the clipboard content
  // is inaccessible at match creation time (e.g. iOS 14).
  AutocompleteMatch NewBlankURLMatch();

  // Returns a text match with no text. This can be used if the clipboard
  // content is inaccessible at match creation time (e.g. iOS 14).
  AutocompleteMatch NewBlankTextMatch();

  // Returns a image match with no attached image. This can be used if the
  // clipboard content is inaccessible at match creation time (e.g. iOS 14).
  AutocompleteMatch NewBlankImageMatch();

  // If there is a url copied to the clipboard and accessing it will not show a
  // clipboard access notification (e.g. iOS 14), use it to create a match.
  // |read_clipboard_content| will be filled with false if the clipboard didn't
  // have any content (either because there was none or because accessing it
  // would have shown a clipboard access notification, and true if there was
  // content.
  std::optional<AutocompleteMatch> CreateURLMatch(
      const AutocompleteInput& input,
      bool* read_clipboard_content);
  // If there is text copied to the clipboard and accessing it will not show a
  // clipboard access notification (e.g. iOS 14), use it to create a match.
  // |read_clipboard_content| will be filled with false if the clipboard didn't
  // have any content (either because there was none or because accessing it
  // would have shown a clipboard access notification, and true if there was
  // content.
  std::optional<AutocompleteMatch> CreateTextMatch(
      const AutocompleteInput& input,
      bool* read_clipboard_content);
  // If there is an image copied to the clipboard and accessing it will not show
  // a clipboard access notification (e.g. iOS 14), use it to create a match.
  // The image match is asynchronous (because constructing the image post data
  // takes time), so instead of returning an optional match like the other
  // Create functions, it returns a boolean indicating whether there will be a
  // match.
  bool CreateImageMatch(const AutocompleteInput& input);

  // Handles the callback response from |CreateImageMatch| and turns the image
  // into an AutocompleteMatch.
  void CreateImageMatchCallback(const AutocompleteInput& input,
                                const base::TimeDelta clipboard_contents_age,
                                std::optional<gfx::Image>);
  // Handles the callback response from |CreateImageMatchCallback| and adds the
  // created AutocompleteMatch to the matches list.
  void AddImageMatchCallback(const AutocompleteInput& input,
                             const base::TimeDelta clipboard_contents_age,
                             std::optional<AutocompleteMatch> match);

  // Resize and encode the image data into bytes. This can take some time if the
  // image is large, so this should happen on a background thread.
  static scoped_refptr<base::RefCountedMemory> EncodeClipboardImage(
      gfx::ImageSkia image);
  // Construct the actual image match once the image has been encoded into
  // bytes. This should be called back on the main thread.
  void ConstructImageMatchCallback(
      ClipboardImageMatchCallback callback,
      scoped_refptr<base::RefCountedMemory> image_bytes);

  // TODO(crbug.com/40176141): OmniboxViewIOS should use following functions
  // instead their own implementations.
  // Called when url data is received from clipboard for creating match with
  // content.
  void OnReceiveURLForMatchWithContent(ClipboardMatchCallback callback,
                                       AutocompleteMatch* match,
                                       std::optional<GURL> optional_gurl);

  // Called when text data is received from clipboard for creating match with
  // content.
  void OnReceiveTextForMatchWithContent(
      ClipboardMatchCallback callback,
      AutocompleteMatch* match,
      std::optional<std::u16string> optional_text);

  // Called when image data is received from clipboard for creating match with
  // content.
  void OnReceiveImageForMatchWithContent(
      ClipboardMatchCallback callback,
      AutocompleteMatch* match,
      std::optional<gfx::Image> optional_image);

  // Called when image match is received from clipboard for creating match with
  // content.
  void OnReceiveImageMatchForMatchWithContent(
      ClipboardMatchCallback callback,
      AutocompleteMatch* match,
      std::optional<AutocompleteMatch> optional_match);

  // Updated clipboard |match| with |url|.
  void UpdateClipboardURLContent(const GURL& url, AutocompleteMatch* match);

  // Updated clipboard |match| with |text|.
  bool UpdateClipboardTextContent(const std::u16string& text,
                                  AutocompleteMatch* match);

  // Update the timestamp of the most recently used clipboard suggestion to the
  // timestamp provided by the ui::Clipboard instance.
  void UpdateMostRecentlyUsedClipboardSuggestionTimestamp();

  raw_ptr<AutocompleteProviderClient> client_;
  raw_ptr<ClipboardRecentContent> clipboard_content_;

  // The current URL suggested and the number of times it has been offered.
  // Used for recording metrics.
  GURL current_url_suggested_;
  size_t current_url_suggested_times_;

  // Used to cancel image construction callbacks if autocomplete Stop() is
  // called.
  base::WeakPtrFactory<ClipboardProvider> callback_weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_CLIPBOARD_PROVIDER_H_
