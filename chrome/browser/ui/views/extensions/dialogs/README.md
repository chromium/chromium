## Extensions Dialogs

### Steps for creating a new dialog

1. If the dialog is called from code outside views, which most likely is, add
`Show<Name>Dialog` method to `chrome/browser/ui/extensions/extensions_dialogs.h`
2. Implement `Show<NameDialog` in `chrome/browser/ui/views/extensions`:

- Method should receive all the information to display in the UI, and should not compute any extensions logic (e.g pass `extension_site_access` value instead of computing the site access here).
- Use `ui::DialogModel::Builder` to create the dialog. See more information on
 `ui/base/models/dialog_model.h`
- Show the dialog using a util method from `chrome/browser/ui/views/extensions/extensions_dialogs_utils.h`

```
namespace extensions {

void Show<Name>Dialog(...) {
  ui::DialogModel::Builder
    .SetTitle(...)
    // Set the dialog information. If necessary, use ui::DialogModelDelegate.

  ShowDialog(...)
}

}  // namespace
```

3. Add interactive tests using Kombucha. See more information on `chrome/test/interaction/README.md`
