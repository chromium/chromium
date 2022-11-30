# How to integrate the AutofillHintsService

1. Add all files of this directory to your project.
2. Get the binder in your autofill service as below

    ```java
    public void processNode(AssistStructure.ViewNode node) {
        Bundle bundle = node.getExtras();
        if (bundle != null) {
            IBinder binder = bundle.getBinder("AUTOFILL_HINTS_SERVICE");
            if (binder != null) {
                callViewTypeService(binder);
            } else {
                Log.e("MyAutofillService", "binder is null.");
            }
        } else {
            Log.e("MyAutofillService", "bundle is null.");
        }
    }
    ```

3. Register the ViewTypeCallback

    ```java
    private void callViewTypeService(IBinder binder) {
        IViewTypeService viewTypeService = IViewTypeService.Stub.asInterface(binder);
        if (viewTypeService != null) {
            try {
                if (mViewTypeCallback == null) mViewTypeCallback = new ViewTypeCallback();
                viewTypeService.registerViewTypeCallback(mViewTypeCallback.getBinder());
                Log.d("MyAutofillService", " registerViewTypeCallback ");
            } catch (Exception e) {
                Log.e("MyAutofillService", " registerViewTypeCallback exception", e);
            }
        } else {
            Log.e("MyAutofillService", "viewTypeService is null.");
        }
    }
    ```

4. A list of ViewType will be returned from ViewTypeCallback when they are available.

    ```java
    public void onViewTypeAvailable(List<ViewType> viewTypeList) {
        for(ViewType viewType : viewTypeList) {
          if (viewType.getServerPredictions() ! = null) {
              // Uses server predictions if they are available.
          } else {
              // otherwise, uses viewType.mServerType.
          }
        }
    }
    ```
