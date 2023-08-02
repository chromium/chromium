// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/select_file_dialog_bridge.h"

#include <AppKit/AppKit.h>
#include <CoreServices/CoreServices.h>  // pre-macOS 11
#include <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>  // macOS 11
#include <stddef.h>

#include "base/apple/bridging.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/thread_restrictions.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const int kFileTypePopupTag = 1234;

// TODO(macOS 11): Remove this.
CFStringRef CreateUTIFromExtension(const base::FilePath::StringType& ext) {
  base::ScopedCFTypeRef<CFStringRef> ext_cf(base::SysUTF8ToCFStringRef(ext));
  return UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                               ext_cf.get(), nullptr);
}

NSString* GetDescriptionFromExtension(const base::FilePath::StringType& ext) {
  if (@available(macOS 11, *)) {
    UTType* type =
        [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext)];
    NSString* description = type.localizedDescription;

    if (description.length) {
      return description;
    }
  } else {
    base::ScopedCFTypeRef<CFStringRef> uti(CreateUTIFromExtension(ext));
    NSString* description =
        base::apple::CFToNSOwnershipCast(UTTypeCopyDescription(uti.get()));

    if (description && description.length) {
      return description;
    }
  }

  // In case no description is found, create a description based on the
  // unknown extension type (i.e. if the extension is .qqq, the we create
  // a description "QQQ File (.qqq)").
  std::u16string ext_name = base::UTF8ToUTF16(ext);
  return l10n_util::GetNSStringF(IDS_APP_SAVEAS_EXTENSION_FORMAT,
                                 base::i18n::ToUpper(ext_name), ext_name);
}

NSView* CreateAccessoryView() {
  // The label. Add attributes per-OS to match the labels that macOS uses.
  NSTextField* label =
      [NSTextField labelWithString:l10n_util::GetNSString(
                                       IDS_SAVE_PAGE_FILE_FORMAT_PROMPT_MAC)];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.textColor = NSColor.secondaryLabelColor;
  if (base::mac::IsAtLeastOS11())
    label.font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];

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
  CGFloat labelPopupPadding;
  if (base::mac::IsAtLeastOS11())
    labelPopupPadding = 8;
  else
    labelPopupPadding = 5;
  [constraints addObject:[popup.leadingAnchor
                             constraintEqualToAnchor:label.trailingAnchor
                                            constant:labelPopupPadding]];
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
  // since the dialog_ will stay alive longer than this object.
  NSSavePanel* _dialog;

  // Two ivars serving the same purpose. While `_fileTypeLists` is for pre-macOS
  // 11, and contains NSStrings with UTType identifiers, `_fileUTTypeLists` is
  // for macOS 11 and later, and contains UTTypes. TODO(macOS 11): Clean this
  // up.

  // An array where each item is an array of different extensions in an
  // extension group.
  NSArray<NSArray<NSString*>*>* __strong _fileTypeLists;
  NSArray<NSArray<UTType*>*>* __strong _fileUTTypeLists
      API_AVAILABLE(macos(11.0));
}

// TODO(macOS 11): Remove this.
- (instancetype)initWithDialog:(NSSavePanel*)dialog
                 fileTypeLists:(NSArray<NSArray<NSString*>*>*)fileTypeLists;

- (instancetype)initWithDialog:(NSSavePanel*)dialog
               fileUTTypeLists:(NSArray<NSArray<UTType*>*>*)fileUTTypeLists
    API_AVAILABLE(macos(11.0));

- (void)popupAction:(id)sender;
@end

@implementation SelectFileDialogDelegate

- (BOOL)panel:(id)sender validateURL:(NSURL*)url error:(NSError**)outError {
  // Refuse to accept users closing the dialog with a key repeat, since the key
  // may have been first pressed while the user was looking at insecure content.
  // See https://crbug.com/637098.
  if (NSApp.currentEvent.type == NSEventTypeKeyDown &&
      NSApp.currentEvent.ARepeat) {
    return NO;
  }

  return YES;
}

@end

@implementation ExtensionDropdownHandler

// TODO(macOS 11): Remove this.
- (instancetype)initWithDialog:(NSSavePanel*)dialog
                 fileTypeLists:(NSArray<NSArray<NSString*>*>*)fileTypeLists {
  if ((self = [super init])) {
    _dialog = dialog;
    _fileTypeLists = fileTypeLists;
  }
  return self;
}

- (instancetype)initWithDialog:(NSSavePanel*)dialog
               fileUTTypeLists:(NSArray<NSArray<UTType*>*>*)fileUTTypeLists
    API_AVAILABLE(macos(11.0)) {
  if ((self = [super init])) {
    _dialog = dialog;
    _fileUTTypeLists = fileUTTypeLists;
  }
  return self;
}

- (void)popupAction:(id)sender {
  NSUInteger index = [sender indexOfSelectedItem];
  if (@available(macOS 11, *)) {
    if (index < [_fileUTTypeLists count]) {
      // For save dialogs, this causes the first item in the allowedContentTypes
      // array to be used as the extension for the save panel.
      _dialog.allowedContentTypes = [_fileUTTypeLists objectAtIndex:index];
    } else {
      // The user selected "All files" option. (Note that an empty array is "all
      // types" and nil is an error.)
      _dialog.allowedContentTypes = @[];
    }
  } else {
    if (index < [_fileTypeLists count]) {
      // For save dialogs, this causes the first item in the allowedFileTypes
      // array to be used as the extension for the save panel.
      _dialog.allowedFileTypes = [_fileTypeLists objectAtIndex:index];
    } else {
      // The user selected "All files" option. (Note that nil is "all types" and
      // an empty array is an error.)
      _dialog.allowedFileTypes = nil;
    }
  }
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
  if (show_callback_)
    [panel_ cancel:panel_];

  // Balance the setDelegate called during Show.
  [panel_ setDelegate:nil];
}

void SelectFileDialogBridge::Show(
    SelectFileDialogType type,
    const std::u16string& title,
    const base::FilePath& default_path,
    SelectFileTypeInfoPtr file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    PanelEndedCallback initialize_callback) {
  // Never consider the current WatchHangsInScope as hung. There was most likely
  // one created in ThreadControllerWithMessagePumpImpl::DoWork(). The current
  // hang watching deadline is not valid since the user can take unbounded time
  // to select a file. HangWatching will resume when the next task
  // or event is pumped in MessagePumpCFRunLoop so there is no need to
  // reactivate it. You can see the function comments for more details.
  base::HangWatcher::InvalidateActiveExpectations();

  show_callback_ = std::move(initialize_callback);
  type_ = type;
  // Note: we need to retain the dialog as |owning_window_| can be null.
  // (See https://crbug.com/29213 .)
  if (type_ == SelectFileDialogType::kSaveAsFile)
    panel_ = [NSSavePanel savePanel];
  else
    panel_ = [NSOpenPanel openPanel];
  g_last_created_panel_for_testing = panel_;

  if (!title.empty())
    panel_.message = base::SysUTF16ToNSString(title);

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
  } else {
    // This does not use ObjCCast because the underlying object could be a
    // non-exported AppKit type (https://crbug.com/995476).
    NSOpenPanel* open_dialog = static_cast<NSOpenPanel*>(panel_);

    if (type_ == SelectFileDialogType::kOpenMultiFile)
      open_dialog.allowsMultipleSelection = YES;
    else
      open_dialog.allowsMultipleSelection = NO;

    if (type_ == SelectFileDialogType::kFolder ||
        type_ == SelectFileDialogType::kUploadFolder ||
        type_ == SelectFileDialogType::kExistingFolder) {
      open_dialog.canChooseFiles = NO;
      open_dialog.canChooseDirectories = YES;

      if (type_ == SelectFileDialogType::kFolder)
        open_dialog.canCreateDirectories = YES;
      else
        open_dialog.canCreateDirectories = NO;

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
  if (default_dir)
    panel_.directoryURL = [NSURL fileURLWithPath:default_dir];
  if (default_filename)
    panel_.nameFieldStringValue = default_filename;

  // Ensure that |callback| (rather than |this|) be retained by the block.
  auto callback = base::BindRepeating(&SelectFileDialogBridge::OnPanelEnded,
                                      weak_factory_.GetWeakPtr());
  [panel_ beginSheetModalForWindow:owning_window_
                 completionHandler:^(NSInteger result) {
                   callback.Run(result != NSModalResponseOK);
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
  // extensions in an extension group. TODO(macOS 11): Remove the first,
  // uncomment the second.
  NSMutableArray<NSArray<NSString*>*>* file_type_lists = [NSMutableArray array];
  NSMutableArray /*<NSArray<UTType*>*>*/* file_uttype_lists =
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

    // Store different extensions in the current extension group. TODO(macOS
    // 11): Remove the first, uncomment the second.
    NSMutableArray<NSString*>* file_type_array = [NSMutableArray array];
    NSMutableArray /*<UTType*>*/* file_uttype_array = [NSMutableArray array];
    for (const base::FilePath::StringType& ext : ext_list) {
      if (ext == default_extension) {
        default_extension_index = i;
      }

      if (@available(macOS 11, *)) {
        UTType* type =
            [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext)];
        // If the extension string is invalid (e.g. contains dots), it's not a
        // valid extension and `type` will be nil. In that case, invent a type
        // that doesn't match any real files. When passed to the file picker, no
        // files will be allowed to be selected. This matches the pre-UTTypes
        // behavior in which an invalid specified type did not allow any files
        // to be selected, and this matches Firefox and Safari behavior (see
        // https://crbug.com/1423362#c17 for a test case).
        if (!type) {
          NSString* identifier =
              [NSString stringWithFormat:@"org.chromium.not-a-real-type.%@",
                                         [NSUUID UUID].UUIDString];
          type = [UTType importedTypeWithIdentifier:identifier];
        }

        if (![file_uttype_array containsObject:type]) {
          [file_uttype_array addObject:type];
        }
      } else {
        // Crash reports suggest that CreateUTIFromExtension may return nil.
        // Hence we nil check before adding to |file_type_set|. See
        // crbug.com/630101 and rdar://27490414.
        NSString* uti =
            base::apple::CFToNSOwnershipCast(CreateUTIFromExtension(ext));
        if (uti) {
          if (![file_type_array containsObject:uti]) {
            [file_type_array addObject:uti];
          }
        }

        // Always allow the extension itself, in case the UTI doesn't map
        // back to the original extension correctly. This occurs with dynamic
        // UTIs on 10.7 and 10.8.
        // See https://crbug.com/148840, https://openradar.appspot.com/12316273
        NSString* ext_ns = base::SysUTF8ToNSString(ext);
        if (![file_type_array containsObject:ext_ns]) {
          [file_type_array addObject:ext_ns];
        }
      }
    }

    if (@available(macOS 11, *)) {
      [file_uttype_lists addObject:file_uttype_array];
    } else {
      [file_type_lists addObject:file_type_array];
    }
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

  if (@available(macOS 11, *)) {
    extension_dropdown_handler_ =
        [[ExtensionDropdownHandler alloc] initWithDialog:panel_
                                         fileUTTypeLists:file_uttype_lists];
  } else {
    extension_dropdown_handler_ =
        [[ExtensionDropdownHandler alloc] initWithDialog:panel_
                                           fileTypeLists:file_type_lists];
  }

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
  if (!show_callback_)
    return;

  int index = 0;
  std::vector<base::FilePath> paths;
  if (!did_cancel) {
    if (type_ == SelectFileDialogType::kSaveAsFile) {
      NSURL* url = [panel_ URL];
      if ([url isFileURL]) {
        paths.push_back(base::mac::NSStringToFilePath([url path]));
      }

      NSView* accessoryView = [panel_ accessoryView];
      if (accessoryView) {
        NSPopUpButton* popup = [accessoryView viewWithTag:kFileTypePopupTag];
        if (popup) {
          // File type indexes are 1-based.
          index = [popup indexOfSelectedItem] + 1;
        }
      } else {
        index = 1;
      }
    } else {
      // This does not use ObjCCast because the underlying object could be a
      // non-exported AppKit type (https://crbug.com/995476).
      NSOpenPanel* open_panel = static_cast<NSOpenPanel*>(panel_);

      for (NSURL* url in open_panel.URLs) {
        if (!url.isFileURL)
          continue;
        NSString* path = url.path;

        // There is a bug in macOS where, despite a request to disallow file
        // selection, files/packages are able to be selected. If indeed file
        // selection was disallowed, drop any files selected.
        // https://crbug.com/1357523, FB11405008
        if (!open_panel.canChooseFiles) {
          BOOL is_directory;
          BOOL exists =
              [[NSFileManager defaultManager] fileExistsAtPath:path
                                                   isDirectory:&is_directory];
          BOOL is_package =
              [[NSWorkspace sharedWorkspace] isFilePackageAtPath:path];
          if (!exists || !is_directory || is_package)
            continue;
        }

        paths.push_back(base::mac::NSStringToFilePath(path));
      }
    }
  }

  std::move(show_callback_).Run(did_cancel, paths, index);
}

// static
NSSavePanel* SelectFileDialogBridge::GetLastCreatedNativePanelForTesting() {
  return g_last_created_panel_for_testing;
}

}  // namespace remote_cocoa
