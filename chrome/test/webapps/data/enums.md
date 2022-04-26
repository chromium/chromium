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

| #Enum Name | Values (* = default) |  |  |  |  |  |  |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Site | SiteA* | SiteB | SiteC | SiteAFoo | SiteABar | SiteWco | SiteIsolatedApp |  |
| InstallableSite | SiteA* | SiteB | SiteAFoo | SiteABar | SiteWco |  |  |
| Scope | SiteARoot |  |  |  |
| Title | SiteA | SiteAUpdated |  |  |  |  |  |
| Color | Red | Green |  |  |  |  |  |
| ProfileClient | Client2* | Client1 |  |  |  |  |  |
| UserDisplayPreference | Standalone | Browser |  |  |  |  |  |
| IsShown | Shown | NotShown |  |  |  |  |  |
| IsOn | On | Off |  |  |  |  |  |
| Display | Standalone | Minimal | Wco |  |  |  |  |
| FileExtension | Txt | Png |  |  |  |  |  |
| Location | StartUrl | FileHandleUrlForTxt | FileHandleUrlForPng |  |  |  |  |
| Number | One | Two |  |  |  |  |  |
| FilesOptions | OneTextFile | MultipleTextFiles | OnePngFile | MultiplePngFiles | AllTestAndPngFiles |  |  |
| AllowDenyOptions | Allow | Deny |
| AskAgainOptions | AskAgain | Remember |
