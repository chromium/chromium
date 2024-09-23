// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_updater.h"

#include <objbase.h>

#include <shobjidl.h>
#include <windows.h>

#include <propkey.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"

namespace {

// Creates an IShellLink object.
// An IShellLink object is almost the same as an application shortcut, and it
// requires three items: the absolute path to an application, an argument
// string, and a title string.
bool AddShellLink(Microsoft::WRL::ComPtr<IObjectCollection> collection,
                  const base::FilePath& application_path,
                  scoped_refptr<ShellLinkItem> item) {
  // Create an IShellLink object.
  Microsoft::WRL::ComPtr<IShellLink> link;
  HRESULT result = ::CoCreateInstance(
      CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
  if (FAILED(result))
    return false;

  // Set the application path.
  // We should exit this function when this call fails because it doesn't make
  // any sense to add a shortcut that we cannot execute.
  result = link->SetPath(application_path.value().c_str());
  if (FAILED(result))
    return false;

  // Attach the command-line switches of this process before the given
  // arguments and set it as the arguments of this IShellLink object.
  // We also exit this function when this call fails because it isn't useful to
  // add a shortcut that cannot open the given page.
  std::wstring arguments(item->GetArguments());
  if (!arguments.empty()) {
    result = link->SetArguments(arguments.c_str());
    if (FAILED(result))
      return false;
  }

  // Attach the given icon path to this IShellLink object.
  // Since an icon is an optional item for an IShellLink object, so we don't
  // have to exit even when it fails.
  if (!item->icon_path().empty()) {
    link->SetIconLocation(item->icon_path().value().c_str(),
                          item->icon_index());
  }

  // Set the title of the IShellLink object.
  // The IShellLink interface does not have any functions which update its
  // title because this interface is originally for creating an application
  // shortcut which doesn't have titles.
  // So, we should use the IPropertyStore interface to set its title.
  Microsoft::WRL::ComPtr<IPropertyStore> property_store;
  result = link.As(&property_store);
  if (FAILED(result))
    return false;

  if (!base::win::SetStringValueForPropertyStore(
          property_store.Get(), PKEY_Title, base::as_wcstr(item->title()))) {
    return false;
  }

  // Add this IShellLink object to the given collection.
  return SUCCEEDED(collection->AddObject(link.Get()));
}

}  // namespace


// ShellLinkItem

ShellLinkItem::ShellLinkItem()
    : command_line_(base::CommandLine::NO_PROGRAM), icon_index_(0) {
}

ShellLinkItem::~ShellLinkItem() {}

std::wstring ShellLinkItem::GetArguments() const {
  return command_line_.GetArgumentsString();
}

base::CommandLine* ShellLinkItem::GetCommandLine() {
  return &command_line_;
}


// JumpListUpdater

JumpListUpdater::JumpListUpdater(const std::wstring& app_user_model_id)
    : app_user_model_id_(app_user_model_id), user_max_items_(0) {}

JumpListUpdater::~JumpListUpdater() {
}

// static
bool JumpListUpdater::IsEnabled() {
  // Do not create custom JumpLists in tests. See http://crbug.com/389375.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTestType);
}

bool JumpListUpdater::BeginUpdate() {
  // This instance is expected to be one-time-use only.
  DCHECK(!destination_list_.Get());

  // Check preconditions.
  if (!JumpListUpdater::IsEnabled() || app_user_model_id_.empty())
    return false;

  // Create an ICustomDestinationList object and attach it to our application.
  HRESULT result =
      ::CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&destination_list_));
  if (FAILED(result))
    return false;

  // Set the App ID for this JumpList.
  result = destination_list_->SetAppID(app_user_model_id_.c_str());
  if (FAILED(result))
    return false;

  // Start a transaction that updates the JumpList of this application.
  // This implementation just replaces the all items in this JumpList, so
  // we don't have to use the IObjectArray object returned from this call.
  // It seems Windows 7 RC (Build 7100) automatically checks the items in this
  // removed list and prevent us from adding the same item.
  UINT max_slots;
  Microsoft::WRL::ComPtr<IObjectArray> removed;
  result = destination_list_->BeginList(&max_slots, IID_PPV_ARGS(&removed));
  if (FAILED(result))
    return false;

  user_max_items_ = max_slots;

  return true;
}

bool JumpListUpdater::CommitUpdate() {
  if (!destination_list_.Get())
    return false;

  // Commit this transaction and send the updated JumpList to Windows.
  return SUCCEEDED(destination_list_->CommitList());
}

bool JumpListUpdater::AddTasks(const ShellLinkItemList& link_items) {
  if (!destination_list_.Get())
    return false;

  // Retrieve the absolute path to "chrome.exe".
  base::FilePath application_path;
  if (!base::PathService::Get(base::FILE_EXE, &application_path))
    return false;

  // Create an EnumerableObjectCollection object to be added items of the
  // "Task" category.
  Microsoft::WRL::ComPtr<IObjectCollection> collection;
  HRESULT result =
      ::CoCreateInstance(CLSID_EnumerableObjectCollection, NULL,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&collection));
  if (FAILED(result))
    return false;

  // Add items to the "Task" category.
  for (ShellLinkItemList::const_iterator it = link_items.begin();
       it != link_items.end(); ++it) {
    if (!AddShellLink(collection, application_path, *it))
      return false;
  }

  // We can now add the new list to the JumpList.
  // ICustomDestinationList::AddUserTasks() also uses the IObjectArray
  // interface to retrieve each item in the list. So, we retrieve the
  // IObjectArray interface from the EnumerableObjectCollection object.
  Microsoft::WRL::ComPtr<IObjectArray> object_array;
  result = collection.As(&object_array);
  if (FAILED(result))
    return false;

  return SUCCEEDED(destination_list_->AddUserTasks(object_array.Get()));
}

bool JumpListUpdater::AddCustomCategory(const std::u16string& category_name,
                                        const ShellLinkItemList& link_items,
                                        size_t max_items) {
  if (!destination_list_.Get())
    return false;

  // Retrieve the absolute path to "chrome.exe".
  base::FilePath application_path;
  if (!base::PathService::Get(base::FILE_EXE, &application_path))
    return false;

  // Exit this function when the given vector does not contain any items
  // because an ICustomDestinationList::AppendCategory() call fails in this
  // case.
  if (link_items.empty() || !max_items)
    return true;

  // Create an EnumerableObjectCollection object.
  // We once add the given items to this collection object and add this
  // collection to the JumpList.
  Microsoft::WRL::ComPtr<IObjectCollection> collection;
  HRESULT result =
      ::CoCreateInstance(CLSID_EnumerableObjectCollection, NULL,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&collection));
  if (FAILED(result))
    return false;

  for (ShellLinkItemList::const_iterator item = link_items.begin();
       item != link_items.end() && max_items > 0; ++item, --max_items) {
    if (!AddShellLink(collection, application_path, *item))
      return false;
  }

  // We can now add the new list to the JumpList.
  // The ICustomDestinationList::AppendCategory() function needs the
  // IObjectArray interface to retrieve each item in the list. So, we retrive
  // the IObjectArray interface from the IEnumerableObjectCollection object
  // and use it.
  // It seems the ICustomDestinationList::AppendCategory() function just
  // replaces all items in the given category with the ones in the new list.
  Microsoft::WRL::ComPtr<IObjectArray> object_array;
  result = collection.As(&object_array);
  if (FAILED(result))
    return false;

  return SUCCEEDED(destination_list_->AppendCategory(
      base::as_wcstr(category_name), object_array.Get()));
}

// static
bool JumpListUpdater::DeleteJumpList(const std::wstring& app_user_model_id) {
  if (!JumpListUpdater::IsEnabled() || app_user_model_id.empty())
    return false;

  Microsoft::WRL::ComPtr<ICustomDestinationList> destination_list;
  return SUCCEEDED(::CoCreateInstance(CLSID_DestinationList, nullptr,
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&destination_list))) &&
         SUCCEEDED(destination_list->DeleteList(app_user_model_id.c_str()));
}
