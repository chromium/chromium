## WebUI Web Apps

### Installable WebUIs

Chrome WebUI can be installed as web apps if they are developed for that purpose (e.g. have valid manifests). Please consult with the Web Apps & Security teams if you consider making a WebUI installable.
So far the only such app is the PasswordManager WebUI available at `chrome://password-manager`.

### Restrictions

WebUI web apps should have a clear and limited purpose and scope, and do not access other Chrome resources. For example, a Password Manager app should not let the user access the browsing history inside the app.
To guarantee that, a navigation throttle exists that opens all navigations from WebUI web apps in a new browser tab, instead of opening an in-app CCT.

*This does not apply for ChromeOS system web apps, which might have their own navigation rules.
