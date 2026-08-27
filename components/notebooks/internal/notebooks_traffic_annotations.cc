// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_traffic_annotations.h"

namespace notebooks {

net::NetworkTrafficAnnotationTag GetCreateNotebookTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation(
      "notebooks_service_create_notebook",
      R"(
      semantics {
        sender: "Notebooks Service Create Notebook"
        description:
          "Chrome feature that creates a notebook, which is a container for "
          "tab and user-uploaded sources providing functionality for users "
          "to make queries and generate artifacts based on those sources."
        trigger: "User upgrades tab group to notebook."
        data:
          "The OAuth token for the signed in account and the user-defined "
          "display name for the notebook."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
          type: USER_CONTENT
        }
        last_reviewed: "2026-07-15"
        internal {
          contacts {
            email: "chrome-ai-productivity-eng@google.com"
          }
          contacts {
            email: "woodchip@chromium.org"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled in Chrome settings by toggling off "
          "'History and tabs' under 'You and Google' > 'In your Google "
          "Account'."
        chrome_policy {
          SyncDisabled {
            SyncDisabled: true
          }
        }
        chrome_policy {
          SyncTypesListDisabled {
            SyncTypesListDisabled {
              entries: "tabs"
            }
          }
        }
        chrome_policy {
          GenAiDefaultSettings {
            GenAiDefaultSettings: 2
          }
        }
      })");
}

net::NetworkTrafficAnnotationTag GetCreateNotebookSourceTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation(
      "notebooks_service_create_notebook_source",
      R"(
      semantics {
        sender: "Notebooks Service Create Notebook Source"
        description:
          "Chrome feature that adds a tab source to a notebook. A notebook is "
          "a container for tab and user-uploaded sources that provides "
          "functionality for users to make queries and generate artifacts "
          "based on said sources."
        trigger: "User opens a new tab inside an existing notebook."
        data:
          "The OAuth token for the signed in account, the tab identifier "
          "string, and the string identifier for the notebook to which the "
          "source is being added."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
          type: SENSITIVE_URL
        }
        last_reviewed: "2026-07-20"
        internal {
          contacts {
            email: "chrome-ai-productivity-eng@google.com"
          }
          contacts {
            email: "woodchip@chromium.org"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled in Chrome settings by toggling off "
          "'History and tabs' under 'You and Google' > 'In your Google "
          "Account'."
        chrome_policy {
          SyncDisabled {
            SyncDisabled: true
          }
        }
        chrome_policy {
          SyncTypesListDisabled {
            SyncTypesListDisabled {
              entries: "tabs"
            }
          }
        }
        chrome_policy {
          GenAiDefaultSettings {
            GenAiDefaultSettings: 2
          }
        }
      })");
}

net::NetworkTrafficAnnotationTag GetListNotebooksForUserTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation(
      "notebooks_service_list_notebooks_for_user",
      R"(
      semantics {
        sender: "Notebooks Service List Notebooks for User"
        description:
          "Chrome feature that lists basic information, such as display name,"
          "about all the notebooks owned by the signed-in user. Notebooks are "
          "containers for tab and user-uploaded sources providing "
          "functionality for users to make queries and generate artifacts "
          "based on those sources."
        trigger:
          "User hovers over 'Notebooks' in the Chrome menu or over 'add tabs "
          "to Notebook' option on tab right-click menu, or opens Chrome left "
          "rail, and notebooks list is not already cached. "
        data:
          "The OAuth token for the signed in account."
        destination: GOOGLE_OWNED_SERVICE
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2026-07-29"
        internal {
          contacts {
            email: "chrome-ai-productivity-eng@google.com"
          }
          contacts {
            email: "woodchip@chromium.org"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature can be disabled in Chrome settings by toggling off "
          "'History and tabs' under 'You and Google' > 'In your Google "
          "Account'."
        chrome_policy {
          SyncDisabled {
            SyncDisabled: true
          }
        }
        chrome_policy {
          SyncTypesListDisabled {
            SyncTypesListDisabled {
              entries: "tabs"
            }
          }
        }
        chrome_policy {
          GenAiDefaultSettings {
            GenAiDefaultSettings: 2
          }
        }
      })");
}

}  // namespace notebooks
