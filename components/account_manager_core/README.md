# //components/account_manager_core

This component holds the core data structures and public facing interfaces for
Chrome OS Account Manager.

The build targets contained in this component can be depended on by Ash (Chrome
OS) and Lacros (Chrome browser) both. Ash and Lacros's common dependencies
must be placed here, to avoid cyclic dependencies between Ash and Lacros.

Currently this component exposes an interface for a facade for Account Manager -
`AccountManagerFacade` - and data structures for Chrome OS accounts.

Also, see:
- `//chrome/browser/lacros/account_manager/`
- `//chromeos/ash/components/account_manager/`
