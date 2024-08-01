// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/select_file_dialog_bridge.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <stddef.h>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#import "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/thread_restrictions.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "ui/base/l10n/l10n_util_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const int kFileTypePopupTag = 1234;

// Returns whether the Uniform Type system considers `ext` to be a valid file
// extension.
bool IsValidExtension(const base::FilePath::StringType& ext) {
  UTType* type =
      [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext)];
  return !!type;
}

NSString* GetDescriptionFromExtension(const base::FilePath::StringType& ext) {
  UTType* type =
      [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext)];
  NSString* description = type.localizedDescription;

  if (description.length) {
    return description;
  }

  // In case no description is found, create a description based on the
  // unknown extension type (i.e. if the extension is .qqq, the we create
  // a description "QQQ File (.qqq)").
  std::u16string ext_name = base::UTF8ToUTF16(ext);
  return l10n_util::GetNSStringF(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                 base::i18n::ToUpper(ext_name), ext_name);
}

NSView* CreateAccessoryView() {
  // The label.
  NSTextField* label =
      [NSTextField labelWithString:l10n_util::GetNSString(
                                       IDS_SAVE_PAGE_FILE_FORMAT_PROMPT_MAC)];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = NSColor.secondaryLabelColor;
  label.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];

  // The popup.
  NSPopUpButton* popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect
                                                    pullsDown:NO];
  popup.translatesAutoresizingMaskIntoConstraints = NO;
  popup.tag = kFileTypePopupTag;

  // A view to group the label and popup together. The top-level view used as
  // the accessory view will be stretched horizontally to match the width of
  // the dialog, and the label and popup need to be grouped together as one
  // view to do centering within it, so use a view to group the label and
  // popup.
  NSView* group = [[NSView alloc] initWithFrame:NSZeroRect];
  group.translatesAutoresizingMaskIntoConstraints = NO;
  [group addSubview:label];
  [group addSubview:popup];

  // This top-level view will be forced by the system to have the width of the
  // save dialog.
  NSView* view = [[NSView alloc] initWithFrame:NSZeroRect];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:group];

  NSMutableArray* constraints = [NSMutableArray array];

  // The required constraints for the group, instantiated top-to-bottom:
  // ┌───────────────────┐
  // │             ↕︎     │
  // │ ↔︎ label ↔︎ popup ↔︎ │
  // │             ↕︎     │
  // └───────────────────┘

  // Top.
  [constraints
      addObject:[popup.topAnchor constraintEqualToAnchor:group.topAnchor
                                                constant:10]];

  // Leading.
  [constraints
      addObject:[label.leadingAnchor constraintEqualToAnchor:group.leadingAnchor
                                                    constant:10]];

  // Horizontal and vertical baseline between the label and popup.
  [constraints addObject:[popup.leadingAnchor
                             constraintEqualToAnchor:label.trailingAnchor
                                            constant:8]];
  [constraints
      addObject:[popup.firstBaselineAnchor
                    constraintEqualToAnchor:label.firstBaselineAnchor]];

  // Trailing.
  [constraints addObject:[group.trailingAnchor
                             constraintEqualToAnchor:popup.trailingAnchor
                                            constant:10]];

  // Bottom.
  [constraints
      addObject:[group.bottomAnchor constraintEqualToAnchor:popup.bottomAnchor
                                                   constant:10]];

  // Then the constraints centering the group in the accessory view. Vertical
  // spacing is fully specified, but as the horizontal size of the accessory
  // view will be forced to conform to the save dialog, only specify horizontal
  // centering.
  // ┌──────────────┐
  // │      ↕︎       │
  // │   ↔group↔︎    │
  // │      ↕︎       │
  // └──────────────┘

  // Top.
  [constraints
      addObject:[group.topAnchor constraintEqualToAnchor:view.topAnchor]];

  // Centering.
  [constraints addObject:[group.centerXAnchor
                             constraintEqualToAnchor:view.centerXAnchor]];

  // Bottom.
  [constraints
      addObject:[view.bottomAnchor constraintEqualToAnchor:group.bottomAnchor]];

  [NSLayoutConstraint activateConstraints:constraints];

  return view;
}

NSSavePanel* __weak g_last_created_panel_for_testing = nil;

}  // namespace

// A bridge class to act as the modal delegate to the save/open sheet and send
// the results to the C++ class.
@interface SelectFileDialogDelegate : NSObject <NSOpenSavePanelDelegate>
@end

// Target for NSPopupButton control in file dialog's accessory view.
@interface ExtensionDropdownHandler : NSObject {
 @private
  // The file dialog to which this target object corresponds. Weak reference
  // since the _dialog will stay alive longer than this object.
  NSSavePanel* __weak _dialog;

  // An array where each item is an array of different extensions in an
  // extension group.
  NSArray<NSArray<NSString*>*>* __strong _fileExtensionLists;
}

- (instancetype)initWithDialog:(NSSavePanel*)dialog
            fileExtensionLists:
                (NSArray<NSArray<NSString*>*>*)fileExtensionLists;

- (void)popupAction:(id)sender;
@end

@implementation SelectFileDialogDelegate

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at insecure content.
  // See https://crbug.com/40085079.
  if (NSApp.currentEvent.type == NSEventTypeKeyDown &&
      NSApp.currentEvent.ARepeat) {
    return NO;
  }

  return YES;
}

@end

@implementation ExtensionDropdownHandler

- (instancetype)initWithDialog:(NSSavePanel*)dialog
            fileExtensionLists:
                (NSArray<NSArray<NSString*>*>*)fileExtensionLists {
  if ((self = [super init])) {
    _dialog = dialog;
    _fileExtensionLists = fileExtensionLists;
  }
  return self;
}

- (void)popupAction:(id)sender {
  NSUInteger index = [sender indexOfSelectedItem];

  // When provided UTTypes, NSOpenPanel determines whether files are selectable
  // by conformance, not by strict type matching. For example, with
  // public.plain-text, not only .txt files will be selectable, but .js, .m3u,
  // and .csv files will be as well. With public.zip-archive, not only .zip
  // files will be selectable, but also .jar files and .xlsb files.
  //
  // While this can be great for normal viewing/editing apps, this is not
  // desirable for Chromium, where the web platform requires strict type
  // matching on the provided extensions, and files that have a conforming file
  // type by accident of their implementation shouldn't qualify for selection.
  //
  // Unfortunately, there's no great way to do strict type matching with
  // NSOpenPanel. Setting explicit extensions via -allowedFileTypes is
  // deprecated, and there's no way to specify that strict type equality should
  // be used for -allowedContentTypes (FB13721802).
  //
  // -[NSOpenSavePanelDelegate panel:shouldEnableURL:] could be used to enforce
  // strict type matching, however its presence on the delegate means that all
  // files in the file list start off being displayed as disabled, and slowly
  // become enabled if they qualify. This is non-performant and quite a poor
  // user experience.
  //
  // Therefore, use the deprecated API, because it's the only way to remain
  // performant while achieving strict type matching.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (index < _fileExtensionLists.count && _fileExtensionLists[index].count) {
    // For save dialogs, this causes the first item in the allowedFileTypes
    // array to be used as the extension for the save panel.
    _dialog.allowedFileTypes = _fileExtensionLists[index];
  } else {
    // The user selected "All files" option (or this is the error case where the
    // page specified literally no valid extensions).
    //
    // (API note: nil is "all types" and an empty array is an error, unlike with
    // -allowedContentTypes, where an empty array is "all types" and nil is an
    // error.)
    _dialog.allowedFileTypes = nil;
  }
#pragma clang diagnostic pop
}

@end

namespace remote_cocoa {

using mojom::SelectFileDialogType;
using mojom::SelectFileTypeInfoPtr;

SelectFileDialogBridge::SelectFileDialogBridge(NSWindow* owning_window)
    : owning_window_(owning_window), weak_factory_(this) {}

SelectFileDialogBridge::~SelectFileDialogBridge() {
  // If we never executed our callback, then the panel never closed. Cancel it
  // now.
  if (show_callback_) {
    [panel_ cancel:panel_];
  }

  // Balance the setDelegate called during Show.
  panel_.delegate = nil;
}

void SelectFileDialogBridge::Show(
    SelectFileDialogType type,
    const std::u16string& title,
    const base::FilePath& default_path,
    SelectFileTypeInfoPtr file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    ShowCallback callback) {
  // Never consider the current WatchHangsInScope as hung. There was most likely
  // one created in ThreadControllerWithMessagePumpImpl::DoWork(). The current
  // hang watching deadline is not valid since the user can take unbounded time
  // to select a file. HangWatching will resume when the next task
  // or event is pumped in MessagePumpCFRunLoop so there is no need to
  // reactivate it. You can see the function comments for more details.
  base::HangWatcher::InvalidateActiveExpectations();

  show_callback_ = std::move(callback);
  type_ = type;
  // Note: we need to retain the dialog as |owning_window_| can be null.
  // (See https://crbug.com/41052845.)
  if (type_ == SelectFileDialogType::kSaveAsFile) {
    panel_ = [NSSavePanel savePanel];
  } else {
    panel_ = [NSOpenPanel openPanel];
  }
  g_last_created_panel_for_testing = panel_;

  if (!title.empty()) {
    panel_.message = base::SysUTF16ToNSString(title);
  }

  NSString* default_dir = nil;
  NSString* default_filename = nil;
  if (!default_path.empty()) {
    // The file dialog is going to do a ton of stats anyway. Not much
    // point in eliminating this one.
    base::ScopedAllowBlocking allow_blocking;
    if (base::DirectoryExists(default_path)) {
      default_dir = base::SysUTF8ToNSString(default_path.value());
    } else {
      default_dir = base::SysUTF8ToNSString(default_path.DirName().value());
      default_filename =
          base::SysUTF8ToNSString(default_path.BaseName().value());
    }
  }

  const bool keep_extension_visible =
      file_types ? file_types->keep_extension_visible : false;
  if (type_ != SelectFileDialogType::kFolder &&
      type_ != SelectFileDialogType::kUploadFolder &&
      type_ != SelectFileDialogType::kExistingFolder) {
    if (file_types) {
      SetAccessoryView(
          std::move(file_types), file_type_index, default_extension,
          /*is_save_panel=*/type_ == SelectFileDialogType::kSaveAsFile);
    } else {
      // If no type_ info is specified, anything goes.
      panel_.allowsOtherFileTypes = YES;
    }
  }

  if (type_ == SelectFileDialogType::kSaveAsFile) {
    // When file extensions are hidden and removing the extension from
    // the default filename gives one which still has an extension
    // that macOS recognizes, it will get confused and think the user
    // is trying to override the default extension. This happens with
    // filenames like "foo.tar.gz" or "ball.of.tar.png". Work around
    // this by never hiding extensions in that case.
    base::FilePath::StringType penultimate_extension =
        default_path.RemoveFinalExtension().FinalExtension();
    if (!penultimate_extension.empty() || keep_extension_visible) {
      panel_.extensionHidden = NO;
    } else {
      panel_.extensionHidden = YES;
      panel_.canSelectHiddenExtension = YES;
    }

    // The tag autosetter in macOS is not reliable (see
    // https://crbug.com/41482996). Explicitly set the `showsTagField` property
    // as a signal to macOS that we will handle all the file tagging; a
    // side-effect of setting the property to any value is that it turns off
    // the tag autosetter.
    panel_.showsTagField = YES;
  } else {
    // This does not use ObjCCast because the underlying object could be a
    // non-exported AppKit type (https://crbug.com/41477018).
    NSOpenPanel* open_dialog = static_cast<NSOpenPanel*>(panel_);

    if (type_ == SelectFileDialogType::kOpenMultiFile) {
      open_dialog.allowsMultipleSelection = YES;
    } else {
      open_dialog.allowsMultipleSelection = NO;
    }

    if (type_ == SelectFileDialogType::kFolder ||
        type_ == SelectFileDialogType::kUploadFolder ||
        type_ == SelectFileDialogType::kExistingFolder) {
      open_dialog.canChooseFiles = NO;
      open_dialog.canChooseDirectories = YES;

      if (type_ == SelectFileDialogType::kFolder) {
        open_dialog.canCreateDirectories = YES;
      } else {
        open_dialog.canCreateDirectories = NO;
      }

      NSString* prompt =
          (type_ == SelectFileDialogType::kUploadFolder)
              ? l10n_util::GetNSString(IDS_SELECT_UPLOAD_FOLDER_BUTTON_TITLE)
              : l10n_util::GetNSString(IDS_SELECT_FOLDER_BUTTON_TITLE);
      open_dialog.prompt = prompt;
    } else {
      open_dialog.canChooseFiles = YES;
      open_dialog.canChooseDirectories = NO;
    }

    delegate_ = [[SelectFileDialogDelegate alloc] init];
    open_dialog.delegate = delegate_;
  }
  if (default_dir) {
    panel_.directoryURL = [NSURL fileURLWithPath:default_dir];
  }
  if (default_filename) {
    panel_.nameFieldStringValue = default_filename;
  }

  // Ensure that |callback| (rather than |this|) be retained by the block.
  auto ended_callback = base::BindRepeating(
      &SelectFileDialogBridge::OnPanelEnded, weak_factory_.GetWeakPtr());

  NSWindow* sheet_parent = owning_window_;
  if (NativeWidgetMacNSWindow* sheet_parent_widget_window =
          base::apple::ObjCCast<NativeWidgetMacNSWindow>(sheet_parent)) {
    sheet_parent = [sheet_parent_widget_window preferredSheetParent];
  }
  [panel_ beginSheetModalForWindow:sheet_parent
                 completionHandler:^(NSInteger result) {
                   ended_callback.Run(result != NSModalResponseOK);
                 }];
}

void SelectFileDialogBridge::SetAccessoryView(
    SelectFileTypeInfoPtr file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    bool is_save_panel) {
  DCHECK(file_types);
  NSView* accessory_view = CreateAccessoryView();

  NSPopUpButton* popup = [accessory_view viewWithTag:kFileTypePopupTag];
  DCHECK(popup);

  // Create an array with each item corresponding to an array of different
  // extensions in an extension group.
  NSMutableArray<NSArray<NSString*>*>* file_extension_lists =
      [NSMutableArray array];
  int default_extension_index = -1;
  for (size_t i = 0; i < file_types->extensions.size(); ++i) {
    const std::vector<base::FilePath::StringType>& ext_list =
        file_types->extensions[i];

    // Generate type description for the extension group.
    NSString* type_description = nil;
    if (i < file_types->extension_description_overrides.size() &&
        !file_types->extension_description_overrides[i].empty()) {
      type_description = base::SysUTF16ToNSString(
          file_types->extension_description_overrides[i]);
    } else {
      // No description given for a list of extensions; pick the first one
      // from the list (arbitrarily) and use its description.
      DCHECK(!ext_list.empty());
      type_description = GetDescriptionFromExtension(ext_list[0]);
    }
    DCHECK_NE(0u, [type_description length]);
    [popup addItemWithTitle:type_description];

    // Store different extensions in the current extension group.
    NSMutableArray<NSString*>* file_extensions_array = [NSMutableArray array];
    for (const base::FilePath::StringType& ext : ext_list) {
      // If an extension can't be mapped to a UTType (not even a dynamic one)
      // then attempting to use it with a save panel will cause the save panel
      // service to fail (see https://crbug.com/40900143).
      if (!IsValidExtension(ext)) {
        continue;
      }

      if (ext == default_extension) {
        default_extension_index = i;
      }

      // See -[ExtensionDropdownHandler popupAction:] as to why file extensions
      // are collected here rather than being converted to UTTypes.
      // TODO(FB13721802): Use UTTypes when strict type matching can be
      // specified.
      NSString* ext_ns = base::SysUTF8ToNSString(ext);
      if (![file_extensions_array containsObject:ext_ns]) {
        [file_extensions_array addObject:ext_ns];
      }
    }

    [file_extension_lists addObject:file_extensions_array];
  }

  if (file_types->include_all_files || file_types->extensions.empty()) {
    panel_.allowsOtherFileTypes = YES;
    // If "all files" is specified for a save panel, allow the user to add an
    // alternate non-suggested extension, but don't add it to the popup. It
    // makes no sense to save as an "all files" file type.
    if (!is_save_panel) {
      [popup addItemWithTitle:l10n_util::GetNSString(IDS_APP_SAVEAS_ALL_FILES)];
    }
  }

  extension_dropdown_handler_ =
      [[ExtensionDropdownHandler alloc] initWithDialog:panel_
                                    fileExtensionLists:file_extension_lists];

  // This establishes a weak reference to handler. Hence we persist it as part
  // of `dialog_data_list_`.
  popup.target = extension_dropdown_handler_;
  popup.action = @selector(popupAction:);

  // Note that `file_type_index` uses 1-based indexing.
  if (file_type_index) {
    DCHECK_LE(static_cast<size_t>(file_type_index),
              file_types->extensions.size());
    DCHECK_GE(file_type_index, 1);
    [popup selectItemAtIndex:file_type_index - 1];
  } else if (!default_extension.empty() && default_extension_index != -1) {
    [popup selectItemAtIndex:default_extension_index];
  } else {
    // Select the first item.
    [popup selectItemAtIndex:0];
  }
  [extension_dropdown_handler_ popupAction:popup];

  // There's no need for a popup unless there are at least two choices.
  if (popup.numberOfItems >= 2) {
    panel_.accessoryView = accessory_view;
  }
}

void SelectFileDialogBridge::OnPanelEnded(bool did_cancel) {
  if (!show_callback_) {
    return;
  }

  int index = 0;
  std::vector<base::FilePath> paths;
  std::vector<std::string> file_tags;
  if (!did_cancel) {
    if (type_ == SelectFileDialogType::kSaveAsFile) {
      NSURL* url = panel_.URL;
      if (url.isFileURL) {
        paths.push_back(base::apple::NSURLToFilePath(url));
      }

      NSView* accessoryView = panel_.accessoryView;
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = popup.indexOfSelectedItem + 1;
        }
      } else {
        index = 1;
      }

      // The tag autosetter was turned off when `showsTagField` was set above.
      // Retrieve the tags for assignment later.
      for (NSString* tag in panel_.tagNames) {
        file_tags.push_back(base::SysNSStringToUTF8(tag));
      }
    } else {
      // This does not use ObjCCast because the underlying object could be a
      // non-exported AppKit type (https://crbug.com/41477018).
      NSOpenPanel* open_panel = static_cast<NSOpenPanel*>(panel_);

      for (NSURL* url in open_panel.URLs) {
        if (!url.isFileURL) {
          continue;
        }
        NSString* path = url.path;

        // There is a bug in macOS where, despite a request to disallow file
        // selection, files/packages are able to be selected. If indeed file
        // selection was disallowed, drop any files selected.
        // https://crbug.com/40861123, FB11405008
        if (!open_panel.canChooseFiles) {
          BOOL is_directory;
          BOOL exists =
              [NSFileManager.defaultManager fileExistsAtPath:path
                                                 isDirectory:&is_directory];
          BOOL is_package =
              [NSWorkspace.sharedWorkspace isFilePackageAtPath:path];
          if (!exists || !is_directory || is_package) {
            continue;
          }
        }

        paths.push_back(base::apple::NSStringToFilePath(path));
      }
    }
  }

  std::move(show_callback_).Run(did_cancel, paths, index, file_tags);
}

// static
NSSavePanel* SelectFileDialogBridge::GetLastCreatedNativePanelForTesting() {
  return g_last_created_panel_for_testing;
}

}  // namespace remote_cocoa
