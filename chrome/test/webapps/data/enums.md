# Critical User Journey Action Catalog for the dPWA product

This file catalogs all of the actions that can be used to build critical user journeys for the dPWA product.

Existing documentation lives [here](/docs/webapps/integration-testing-framework.md).

TODO(dmurph): Move more documentation here. https://crbug.com/1314822

## How this file is parsed

The tables in this file are parsed as action templates for critical user journeys. Lines are considered an action template if:
- The first non-whitespace character is a `|`
- Splitting the line using the `|` character as a delimiter, the first item (stripping whitespace):
  - Does not start with `#`
  - Is not `---`
  - Is not empty


## Enums Table

| #Enum Name | Values (* = default) |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Site | Standalone* | MinimalUi | Tabbed | NotPromotable | StandaloneNestedA | StandaloneNestedB | Wco | Isolated | FileHandler | NotInstalled | StandaloneNotStartUrl | Screenshots | HasSubApps | SubApp1 | SubApp2 | TabbedWithHomeTab | TabbedNestedA | TabbedNestedB | TabbedNestedC | ChromeUrl |
| InstallableSite | Standalone* | MinimalUi | Tabbed | StandaloneNestedA | StandaloneNestedB | Wco | NotInstalled | StandaloneNotStartUrl | Screenshots | HasSubApps | SubApp1 | SubApp2 | TabbedWithHomeTab | ChromeUrl |
| Title | StandaloneOriginal | StandaloneUpdated |  |  |  |  |  |
| Color | Red | Green |  |  |  |  |  |
| ProfileClient | Client2* | Client1 |  |  |  |  |  |
| UserDisplayPreference | Standalone | Browser |  |  |  |  |  |
| IsShown | Shown | NotShown |  |  |  |  |  |
| IsOn | On | Off |  |  |  |  |  |
| Display | Standalone | MinimalUi | Tabbed | Wco | Browser |  |  |  |
| FileExtension | Foo | Bar |  |  |  |  |  |
| Location | StartUrl | FileHandleUrlForFoo | FileHandleUrlForBar |  |  |  |  |
| Number | One | Two |  |  |  |  |  |
| FilesOptions | OneFooFile | MultipleFooFiles | OneBarFile | MultipleBarFiles | AllFooAndBarFiles |  |  |
| AllowDenyOptions | Allow | Deny |
| AskAgainOptions | AskAgain | Remember |
| ShortcutOptions | WithShortcut | NoShortcut |
| WindowOptions | Windowed | Browser |
| InstallMode | WebApp* | WebShortcut |
| UpdateDialogResponse | AcceptUpdate | CancelDialogAndUninstall | CancelUninstallAndAcceptUpdate | SkipDialog |
| SubAppInstallDialogOptions | UserAllow* | UserDeny | PolicyOverride |
| ProfileName | Default | Profile2 |
